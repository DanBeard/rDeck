#include "RnsService.h"
#include "WifiService.h"
#include "TimeService.h"
#include "lvgl.h"
#include "RnsUtils/LoraInterface.h"
#include "RnsUtils/TCPClientInterface.h"
#include "Bytes.h"
#include "apps/Settings.h"
#include "RnsUtils/LXMFData.h"
#include <iterator>

//using namespace Retcon::LXMF;

// Yeah this means there can be only 1
static RnsService* rnsService = nullptr; 

RnsService::RnsService(uint8_t id): reticulum({RNS::Type::NONE}),
 identity({RNS::Type::NONE}),
 lxmf_delivery_src({RNS::Type::NONE}),
  rns_fs({RNS::Type::NONE}),
  lora_interface({RNS::Type::NONE}),
  tcp_interface({RNS::Type::NONE}),
   BaseService(id) {
    // make sure there's only 1
    assert(rnsService == nullptr);
    rnsService = this;
}

static void onLinkPacket(const RNS::Bytes& plaintext, const RNS::Packet& packet) {
    Serial.println("[LXMF] *** onLinkPacket callback fired ***");
    Serial.print("[LXMF] Received plaintext size: ");
    Serial.println(plaintext.size());

    if (plaintext.size() < 96) {
        Serial.println("[LXMF] ERROR: plaintext too small for LXMF message (need at least 96 bytes: 16+16+64)");
        return;
    }

    shared_ptr<Retcon::LXMF::Message> lxmf_msg = make_shared<Retcon::LXMF::Message>(plaintext);

    Serial.print("[LXMF] dest: "); Serial.println(lxmf_msg->dest.toHex().c_str());
    Serial.print("[LXMF] src: "); Serial.println(lxmf_msg->src.toHex().c_str());
    Serial.print("[LXMF] packed_payload size: "); Serial.println(lxmf_msg->packed_payload.size());

    // Unpack the LXMF payload to check for service messages
    JsonDocument payloadDoc;
    deserializeMsgPack(payloadDoc, lxmf_msg->packed_payload.data(), lxmf_msg->packed_payload.size());

    // LXMF payload format: [timestamp, title, content, fields]
    // Check if fields contains service message markers
    if (payloadDoc.is<JsonArray>() && payloadDoc.size() >= 4) {
        JsonVariant fieldsVar = payloadDoc[3];
        if (!fieldsVar.isNull() && fieldsVar.is<JsonObject>()) {
            JsonDocument fields;
            fields.set(fieldsVar);

            if (Retcon::Service::ServiceMessage::isServiceMessage(fields)) {
                Serial.println("[LXMF] Detected service message in fields");
                Retcon::Service::ServiceMessage svcMsg = Retcon::Service::ServiceMessage::fromFields(fields);
                rnsService->handleServiceMessage(svcMsg, lxmf_msg->src);
                return;  // Don't process as regular LXMF message
            }
        }
    }

    // Regular LXMF message handling
    lxmf_msg->unpack();  // Extract title/content from packed_payload

    Serial.print("[LXMF] title: '"); Serial.print(lxmf_msg->title.c_str()); Serial.println("'");
    Serial.print("[LXMF] content: '"); Serial.print(lxmf_msg->content.c_str()); Serial.println("'");

    // For received messages, src is the sender (them)
    Retcon::LXMF::addMessageToConversation(*lxmf_msg, lxmf_msg->src);

    // send new message event
    Event e;
    e.src = rnsService;
    e.type = NEW_MESSAGE;
    e.data = lxmf_msg;

    retOsGlobalPtr->publishEvent(e);

}
static void onLink(RNS::Link& link) {
    rnsService->reticulum.should_persist_data();
    Serial.println("[RNS] *** LINK ESTABLISHED ***");
    Serial.printf("[RNS] Link from: %s\n", link.destination().hash().toHex().c_str());
    link.set_packet_callback(onLinkPacket);
}

void RnsService::start(RetOS* retos){
    updateIcon(false);

    // pause and then load config
    RetHal hal = retos->hal();
    _lora = hal.lora;
    _fs = hal.fs;

    // Setup RNS filesystem
    rns_fs = new FileSystem();
    ((FileSystem*)rns_fs.get())->init();
    RNS::Utilities::OS::register_filesystem(rns_fs);

    // Check if WiFi mode is enabled
    JsonObject netSettings = Settings::getSettings(WifiService::settingsSection);
    bool useWifi = netSettings["enabled"] | false;
    const char* tcpHost = netSettings["tcp_host"] | "";
    uint16_t tcpPort = netSettings["tcp_port"] | 4242;
    bool tcpConfigured = strlen(tcpHost) > 0;

    if (useWifi && tcpConfigured) {
        Serial.println("[RNS] WiFi mode enabled, using TCP interface");
        _interfaceMode = InterfaceMode::TCP;
        initTcpInterface();
    } else {
        Serial.println("[RNS] Using LoRa interface");
        _interfaceMode = InterfaceMode::LORA;
        initLoraInterface();
    }

    reticulum = RNS::Reticulum();

    reticulum.transport_enabled(false);
	reticulum.start();

    // Restore known identities from persisted announce data
    {
        const set<Retcon::LXMF::AnnounceData>* announces = Retcon::LXMF::getAnnounceData();
        for(const Retcon::LXMF::AnnounceData &ad : *announces) {
            if(ad.public_key.size() > 0) {
                RNS::Identity::remember(ad.dest, ad.dest, ad.public_key, ad.app_data);
                Serial.print("[RNS] Restored identity for: ");
                Serial.println(ad.dest.toHex().c_str());
            }
        }
    }

    // do we have a saved identity?
    if(!_fs->exists("/reticulum")){
        _fs->mkdir("/reticulum");
    }
    if(!_fs->exists("/reticulum/identity.priv")) {
        // new identity
        Serial.println("[RNS] Creating new Identity...");
        identity = RNS::Identity(true);
        RNS::Bytes priv = identity.get_private_key();
        File priv_file = _fs->open("/reticulum/identity.priv", FILE_WRITE, true);
        // Store hex string in a std::string to avoid dangling pointer
        std::string priv_hex = priv.toHex();

        JsonDocument doc;
        doc["priv_hex"] = priv_hex;
        serializeJson(doc, priv_file);

        //priv_file.write((uint8_t *)priv_hex.c_str(), priv_hex.size());
        priv_file.close();

    } else {
        Serial.println("[RNS] Loading Identity...");
        identity = RNS::Identity(false);
        File priv_file = _fs->open("/reticulum/identity.priv", FILE_READ);
        JsonDocument doc;
        deserializeJson(doc, priv_file);

        RNS::Bytes prv_bytes;
        prv_bytes.assignHex(doc["priv_hex"]);
        identity.load_private_key(prv_bytes);
        priv_file.close();
    }
    // userInfo
    if(!_fs->exists("/reticulum/userinfo.json")) {
        Serial.println("[RNS] Creating new User Info...");
        //new user info
        userInfo["name"] = "New rDeck";
        //serializeJsonPretty(userInfo, Serial);
        saveUserInfo();
    } else {
        Serial.println("[RNS] Loading User Info...");
        File config_file = _fs->open("/reticulum/userinfo.json", FILE_READ);
        deserializeJson(userInfo, config_file);
        //serializeJsonPretty(userInfo, Serial);
        config_file.close();
    }


      // load identity
    //identity = RNS::Identity(false);
    //RNS::Bytes prv_bytes;
    //prv_bytes.assignHex("78E7D93E28D55871608FF13329A226CABC3903A357388A035B360162FF6321570B092E0583772AB80BC425F99791DF5CA2CA0A985FF0415DAB419BBC64DDFAE8");
    //
    _announce_handler = make_shared<RDeckAnnounceHandler>("lxmf.delivery");
    RNS::Transport::register_announce_handler(_announce_handler);

    lxmf_delivery_src = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "lxmf", "delivery");
    lxmf_delivery_src.set_packet_callback(onLinkPacket);
    lxmf_delivery_src.set_link_established_callback(onLink);
    //lxmf_delivery_src.accepts_links(false);
    lxmf_delivery_src.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
    // don't need to manualyl do this
    //RNS::Transport::register_destination(destination);


    

    // HEAD("Registering packet callback with Destination...", RNS::LOG_TRACE);
	// 	destination.set_packet_callback(onPacket);
	// 	destination.set_link_established_callback(onLinkEstablished);
	// 	destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

	// 	{
	// 		// Register PING packet callback
	// 		HEAD("Creating PING Destination instance...", RNS::LOG_TRACE);
	// 		RNS::Destination ping_destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "example_utilities", "echo.request");

	// 		HEAD("Registering packet callback with PING Destination...", RNS::LOG_TRACE);
	// 		ping_destination.set_packet_callback(onPingPacket);
	// 		ping_destination.set_proof_strategy(RNS::Type::Destination::PROVE_NONE);
	// 	}

	// 	HEAD("Registering announce handler with Transport...", RNS::LOG_TRACE);
		//RNS::Transport::register_announce_handler(announce_handler);

    // Initialize trusted servers storage
    Retcon::Service::getTrustedServers().init(_fs);

    // set to running
    _status = RUNNING;
    retos->run_later([this]() {
        Serial.println("RNS ANNOUNCE INITIAL");
        announce();
    }, 1500);

}

void RnsService::initLoraInterface() {
    Serial.println("[RNS] Initializing LoRa interface...");

    lora_interface_impl = new RNS::Interfaces::LoRaInterface(_lora, this);
    lora_interface = RNS::Interface(lora_interface_impl);

    lora_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
    RNS::Transport::register_interface(lora_interface);

    // Default LoRa config
    LoraConfig config {
        .frequency =  914.875F,
        .bandwidth =  250.000F,
        .sf = 7,
        .cr = 5,
        .power = 19,
        .preamble_len = 8,
        .crc = 0,
        .explicitHeader = true
    };
    // Merge any user-configured changes
    mergeLoraSettings(config);

    lora_interface_impl->start(config);
    Serial.println("[RNS] LoRa interface initialized");
}

void RnsService::initTcpInterface() {
    Serial.println("[RNS] Initializing TCP interface...");

    JsonObject netSettings = Settings::getSettings(WifiService::settingsSection);
    const char* tcpHost = netSettings["tcp_host"] | "";
    uint16_t tcpPort = netSettings["tcp_port"] | 4242;

    tcp_interface_impl = new RNS::Interfaces::TCPClientInterface(this);
    tcp_interface = RNS::Interface(tcp_interface_impl);

    tcp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
    RNS::Transport::register_interface(tcp_interface);

    // Check if WiFi is already connected
    WifiService* wifiSvc = _retos->fetchService<WifiService>();
    if (wifiSvc && wifiSvc->isConnected()) {
        Serial.printf("[RNS] WiFi connected, starting TCP connection to %s:%d\n", tcpHost, tcpPort);
        tcp_interface_impl->start(tcpHost, tcpPort);
    } else {
        Serial.println("[RNS] WiFi not connected yet, TCP interface will connect when WiFi is ready");
    }

    Serial.println("[RNS] TCP interface initialized");
}

bool RnsService::isInterfaceOnline() const {
    if (_interfaceMode == InterfaceMode::TCP) {
        return tcp_interface_impl && tcp_interface_impl->isConnected();
    } else {
        return lora_interface_impl && lora_interface.online();
    }
}

void RnsService::saveUserInfo() {
    File config_file = _fs->open("/reticulum/userinfo.json", FILE_WRITE, true);
    serializeJsonPretty(userInfo, config_file);
    config_file.close();
}

void RnsService::announce() {
    if (lxmf_delivery_src) {
		HEAD("Announcing destination...", RNS::LOG_TRACE);
        uint8_t buffer[200];
        JsonDocument doc;

        // Build announce app_data in extended LXMF format: [name_binary, stamp_cost_or_null, device_type]
        // - name_binary: display name as msgpack binary
        // - stamp_cost: null (no stamp required)
        // - device_type: "rdeck" to identify this as an rDeck device
        const char * name = userInfo["name"];
        doc.add(MsgPackBinary(name, strnlen(name, 100)));
        doc.add(nullptr);  // stamp_cost = null (standard LXMF format)
        doc.add("rdeck");  // device_type identifier

        size_t bytesWritten = serializeMsgPack(doc, buffer, 200);
        Serial.println("ANNOUNCE MSGPACK-------");
        Serial.println(name);
        for(int i=0; i< bytesWritten; i++){
            Serial.print((int)buffer[i]); Serial.print(',');
        }
        Serial.println("\n END ANNOUNCE MSGPACK-------");
        TRACE("Interface: announce bytes written = " + std::to_string(bytesWritten) + " ........");

        // Use the active interface
        RNS::Interface& activeInterface = (_interfaceMode == InterfaceMode::TCP) ? tcp_interface : lora_interface;
		lxmf_delivery_src.announce(RNS::bytesFromChunk(buffer, bytesWritten), false, activeInterface);
	}
}

void RnsService::processNextInQueue() {
    PlatformMutexGuard guard(_msg_mutex);
    if(!_sending_message && _send_msg_queue.size() > 0) {
        transmitMsg(_send_msg_queue.front());
        _send_msg_queue.pop();
    } else if(_sending_message && _current_sending_msg &&
              _current_sending_msg->status == Retcon::LXMF::Message::STATUS::RETRY) {
        // Retry the current message
        transmitMsg(_current_sending_msg);
    }
}

void RnsService::queueAction(std::function<void()> action) {
    PlatformMutexGuard guard(_action_mutex);
    _pending_actions.push_back(std::move(action));
}

static unsigned long last_announce = 0;

void RnsService::tick(const unsigned long tMillis) {
    // Drain queued actions from UI thread (rate-limited)
    // Process at most MAX_ACTIONS_PER_TICK to avoid overwhelming Transport
    // with rapid-fire packet sends (e.g., 9 tile requests from Maps).
    // Remaining actions stay queued for the next tick.
    {
        std::vector<std::function<void()>> actions;
        {
            PlatformMutexGuard guard(_action_mutex);
            if (_pending_actions.size() <= MAX_ACTIONS_PER_TICK) {
                actions.swap(_pending_actions);
            } else {
                // Take only the first N actions, leave the rest
                actions.assign(
                    std::make_move_iterator(_pending_actions.begin()),
                    std::make_move_iterator(_pending_actions.begin() + MAX_ACTIONS_PER_TICK)
                );
                _pending_actions.erase(_pending_actions.begin(),
                                       _pending_actions.begin() + MAX_ACTIONS_PER_TICK);
            }
        }
        for (auto& action : actions) {
            try {
                action();
            } catch (...) {}
        }
    }

    // Periodic announce
    if(tMillis - last_announce > (10*60*1000) || tMillis < last_announce){
        Serial.println("RNS ANNOUNCE");
        announce();
        last_announce = tMillis;
        reticulum.should_persist_data();
    }

    reticulum.loop();

    // Tick the active interface
    if (_interfaceMode == InterfaceMode::TCP) {
        if (tcp_interface_impl) {
            // Check if WiFi connected and TCP needs to start
            WifiService* wifiSvc = _retos->fetchService<WifiService>();
            if (wifiSvc && wifiSvc->isConnected() && !tcp_interface_impl->isConnected()) {
                JsonObject netSettings = Settings::getSettings(WifiService::settingsSection);
                const char* tcpHost = netSettings["tcp_host"] | "";
                uint16_t tcpPort = netSettings["tcp_port"] | 4242;
                if (strlen(tcpHost) > 0) {
                    Serial.printf("[RNS] WiFi connected, attempting TCP connection to %s:%d\n", tcpHost, tcpPort);
                    tcp_interface_impl->start(tcpHost, tcpPort);
                }
            }
            tcp_interface_impl->tick(tcp_interface);
        }
    } else {
        if (lora_interface_impl) {
            lora_interface_impl->tick(lora_interface);
        }
    }

    // Process deferred send/retry from receipt callbacks
    if(_needs_send_processing) {
        _needs_send_processing = false;
        processNextInQueue();
    }
}

void RnsService::updateIcon(bool status){
    ServiceIcon iconInfo = {
        .serviceID = this->_id,
        .icon = status ? LV_SYMBOL_UPLOAD : "R?",
        .opacity = status ? LV_OPA_100 : LV_OPA_100,
    };
    _retos->ui()->setServiceIcon(iconInfo);
}

void RnsService::sendMessageUpdateEvent(shared_ptr<Retcon::LXMF::Message> &msg) {
    Event e;
    e.src = this;
    e.type = MESSAGE_UPDATE;
    e.data = msg;

    _retos->publishEvent(e);
}

shared_ptr<Retcon::LXMF::Message> RnsService::sendLxmfMsg(const RNS::Bytes dest, const string &title, const string &contents) {
    shared_ptr<Retcon::LXMF::Message> msg = make_shared<Retcon::LXMF::Message>(lxmf_delivery_src.hash(), dest, title, contents);
    msg->status = Retcon::LXMF::Message::STATUS::QUEUEING;

    // Store in conversation immediately so it shows in the UI
    Retcon::LXMF::addMessageToConversation(*msg, dest);

    {
        PlatformMutexGuard guard(_msg_mutex);
        Serial.println("QUEUEING LXMF MESSAGE");
        if(_send_msg_queue.size() > max_number_queued_msgs) {
            Serial.println("DROPPING LXMF MESSAGE - queue full");
            return msg;
        }
        _send_msg_queue.push(msg);
        // Defer actual transmit to tick() on the services task, avoiding
        // cross-thread calls into Transport::outbound() which can deadlock.
        _needs_send_processing = true;
    }

    // Notify UI so the message appears immediately
    Event e;
    e.src = this;
    e.type = NEW_MESSAGE;
    e.data = msg;
    _retos->publishEvent(e);

    return msg;
}

const queue<shared_ptr<Retcon::LXMF::Message>>& RnsService::queuedMsgs() const {
    return _send_msg_queue;
}

// Receipt callbacks run inside Transport::jobs() where _jobs_running is true.
// We must NOT call transmitMsg (which calls packet.send() → Transport::outbound())
// from here, or outbound() will spinlock waiting for _jobs_running to clear.
// Instead, update state and set a flag for tick() to process.

void transmit_delivery_cb(const RNS::PacketReceipt &receipt) {
    PlatformMutexGuard guard(rnsService->_msg_mutex);

    rnsService->_sending_message = false;
    delete rnsService->_sending_packet;
    rnsService->_sending_packet = nullptr;
    rnsService->_current_sending_msg->status = Retcon::LXMF::Message::STATUS::SENT;
    Serial.println("[LXMF] Message delivery confirmed (ACK received)");
    rnsService->sendMessageUpdateEvent(rnsService->_current_sending_msg);

    // Defer sending next queued message to tick()
    rnsService->_needs_send_processing = true;
}
void transmit_timeout_cb(const RNS::PacketReceipt &receipt) {
    PlatformMutexGuard guard(rnsService->_msg_mutex);

    rnsService->_current_sending_msg->status = Retcon::LXMF::Message::STATUS::RETRY;
    rnsService->sendMessageUpdateEvent(rnsService->_current_sending_msg);

    if(rnsService->_num_retries++ > RnsService::max_number_retries) {
        delete rnsService->_sending_packet;
        rnsService->_sending_packet = nullptr;
        rnsService->_sending_message = false;
        rnsService->_current_sending_msg->status = Retcon::LXMF::Message::STATUS::FAILED;
        Serial.println("[LXMF] Message delivery FAILED after max retries");
        rnsService->sendMessageUpdateEvent(rnsService->_current_sending_msg);
    }
    // else: retry current message

    // Defer retry/next-send to tick()
    rnsService->_needs_send_processing = true;
}
void RnsService::transmitMsg(shared_ptr<Retcon::LXMF::Message>& msg) {
    if(_sending_message && msg != _current_sending_msg) return;
    if (!_sending_message || msg != _current_sending_msg) {
        _num_retries = 0;
    }
    _sending_message = true;

    // find the dest destination and pack the msg
    RNS::Identity their_ident = RNS::Identity::recall(msg->dest);
    if (!their_ident) {
        // No known identity for this destination - can't send without announce
        Serial.println("[RNS] ERROR: No known identity for destination, cannot send");
        msg->status = Retcon::LXMF::Message::STATUS::UNKNOWN_DEST;
        _sending_message = false;
        sendMessageUpdateEvent(msg);
        // Try next message in queue if any
        if(_send_msg_queue.size() > 0) {
            transmitMsg(_send_msg_queue.front());
            _send_msg_queue.pop();
        }
        return;
    }
    RNS::Destination their_dest(their_ident, RNS::Type::Destination::OUT,RNS::Type::Destination::SINGLE, "lxmf",  "delivery");
    msg->pack(lxmf_delivery_src,their_dest);

    _current_sending_msg = msg;
    _current_sending_msg->status = Retcon::LXMF::Message::STATUS::SENDING;
    RNS::Bytes fullMsg = _current_sending_msg->fullMsg();
    // Strip dest_hash (first 16 bytes) for opportunistic single-packet delivery.
    // Python LXMF receivers reconstruct dest from Reticulum packet metadata.
    RNS::Bytes packetData = fullMsg.mid(RNS_HASH_SIZE_BYTES);
    delete _sending_packet;
    _sending_packet = new RNS::Packet(their_dest, packetData);
    _sending_packet->send();
    RNS::PacketReceipt receipt = _sending_packet->receipt();
    receipt.set_timeout(packet_timeout_secs);
    receipt.set_delivery_callback(transmit_delivery_cb);
    receipt.set_timeout_callback(transmit_timeout_cb);

}

static FunctorCallback settingsCallback;
static LoraConfig userConfig;

bool RnsService::drawSettings(lv_obj_t * container, Settings* settings) {
    settings->drawSettingsSectionHeader(container, "Reticulum (LoRa)");

    JsonObject _settings = settings->getSettings(settingsSection);
    if(!_settings.containsKey("lora") || _settings["lora"].isNull()) {
            _settings["lora"].to<JsonObject>();
    }

    BaseLora *radio = retOsGlobalPtr->hal().lora;

    lv_obj_t *fr, *bd, *sf, *cr;

    // Create these BEFORE the functor so the pointers are valid when captured by functor
    LoraConfig &config = radio->config;
    userConfig = config;

    constexpr size_t buf_size = 24;
    char tmp[buf_size + 1];
    snprintf(tmp, buf_size, "%.3f", config.frequency);
    fr = settings->drawSettingsTextInputRow(container, "Frequency", tmp, &settingsCallback);
    snprintf(tmp, buf_size, "%.3f", config.bandwidth);
    bd = settings->drawSettingsTextInputRow(container, "Bandwidth", tmp, &settingsCallback);
    itoa(config.sf, tmp, 10);
    sf = settings->drawSettingsTextInputRow(container, "SF", tmp, &settingsCallback);
    itoa(config.cr, tmp, 10);
    cr = settings->drawSettingsTextInputRow(container, "CR", tmp, &settingsCallback);

    // must be static so it survives past this function call.
    settingsCallback = [_settings, fr, bd, sf, cr](lv_event_t *e){
        lv_obj_t * ta = lv_event_get_target(e);
        // don't pass raw char* to JsonArduino or it won't copy them and you'll get junk later
        String value = lv_textarea_get_text(ta);
        if(ta == fr) {
            userConfig.frequency = value.toFloat();
        } else if(ta == bd) {
             userConfig.bandwidth = value.toFloat();
        } else if(ta == sf) {
             userConfig.sf = value.toInt();
        } else if(ta == cr) {
             userConfig.cr = value.toInt();
        } else {
            Serial.print("Unknown!!");
        }
    };

    return true;
}

void RnsService::applySettings() {
    BaseLora *radio = retOsGlobalPtr->hal().lora;
    LoraConfig old_config = radio->config;

    // did anything actually change?
    Serial.println("Did Lora Change?");
    if(old_config.frequency != userConfig.frequency || old_config.bandwidth != userConfig.bandwidth || old_config.sf != userConfig.sf  || old_config.cr != userConfig.cr) {
        // then change it!
        Serial.println("Lora change detected.... serializing to JSON");
        JsonObject _settings = Settings::getSettings(settingsSection);
        JsonObject loraSettings = _settings["lora"];
        loraSettings["fr"] = userConfig.frequency;
        loraSettings["bw"] = userConfig.bandwidth;
        loraSettings["sf"] = userConfig.sf;
        loraSettings["cr"] = userConfig.cr;

        Serial.println("Lora change detected.... changing radio config");
        if(retOsGlobalPtr->hal().lora != nullptr) {
            retOsGlobalPtr->hal().lora->changeConfig(userConfig);
        }

    }

}

void RnsService::mergeLoraSettings(LoraConfig& config) {
    JsonObject _settings = Settings::getSettings(settingsSection);
    JsonObject loraSettings = _settings["lora"];

    serializeJsonPretty(loraSettings, Serial);

    // copy over any settings changes
    if(loraSettings.containsKey("fr")) {
        config.frequency = loraSettings["fr"];
    }
    if(loraSettings.containsKey("bw")) {
        config.bandwidth = loraSettings["bw"];
    }
    if(loraSettings.containsKey("sf")) {
        config.sf = loraSettings["sf"];
    }
    if(loraSettings.containsKey("cr")) {
        config.cr = loraSettings["cr"];
    }
}

// ============================================================================
// Service Protocol Methods
// ============================================================================

void RnsService::sendServiceMessage(const RNS::Bytes& dest, const Retcon::Service::ServiceMessage& msg) {
    try {
    // Find the destination identity
    RNS::Identity their_ident = RNS::Identity::recall(dest);
    if (!their_ident) {
        Serial.println("[Service] ERROR: No known identity for destination");
        return;
    }

    // Build the LXMF message with service fields
    // We encode the service message into the LXMF fields
    JsonDocument fields;
    msg.toFields(fields);

    // Pack into LXMF format
    // For service messages, we use empty title/content and put data in fields
    time_t timestamp;
    time(&timestamp);

    JsonDocument payload;
    payload.add(timestamp);
    payload.add(MsgPackBinary("", 0));  // empty title
    payload.add(MsgPackBinary("", 0));  // empty content
    payload.add(fields);  // service fields

    uint8_t packed_payload[Retcon::LXMF::Message::max_lxmf_payload_size];
    size_t payload_len = serializeMsgPack(payload, packed_payload, sizeof(packed_payload));

    // Build the full message
    RNS::Destination their_dest(their_ident, RNS::Type::Destination::OUT, RNS::Type::Destination::SINGLE, "lxmf", "delivery");

    RNS::Bytes hashed_part;
    hashed_part.append(their_dest.hash());
    hashed_part.append(lxmf_delivery_src.hash());
    hashed_part.append(packed_payload, payload_len);

    RNS::Bytes hash = RNS::Identity::full_hash(hashed_part);
    RNS::Bytes signed_part;
    signed_part.append(hashed_part);
    signed_part.append(hash);

    RNS::Bytes signature = lxmf_delivery_src.sign(signed_part);

    // Full LXMF message: dest_hash + src_hash + signature + payload
    // But for single-packet delivery, we strip dest_hash
    RNS::Bytes packetData;
    packetData.append(lxmf_delivery_src.hash());
    packetData.append(signature);
    packetData.append(packed_payload, payload_len);

    // Send the packet
    RNS::Packet packet(their_dest, packetData);
    packet.send();

    Serial.printf("[Service] Sent message type 0x%02X to %s\n",
                  static_cast<uint8_t>(msg.msg_type), dest.toHex().substr(0, 12).c_str());

    } catch (...) {}
}

void RnsService::sendTrustAccept(const RNS::Bytes& serverHash) {
    // Get device name from userInfo
    const char* name = userInfo["name"];

    Retcon::Service::TrustAcceptPayload payload;
    payload.device_name = name ? name : "rDeck";

    uint8_t payloadBuf[128];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    Retcon::Service::ServiceMessage msg;
    msg.msg_type = Retcon::Service::MessageType::TRUST_ACCEPT;
    msg.service = "trust";
    msg.payload.assign(payloadBuf, payloadLen);
    msg.request_id = (uint32_t)(millis() & 0xFFFFFFFF);

    sendServiceMessage(serverHash, msg);
    Serial.printf("[Service] Sent TRUST_ACCEPT to %s\n", serverHash.toHex().substr(0, 12).c_str());
}

void RnsService::requestNtpSync(const RNS::Bytes& serverHash) {
    Retcon::Service::NTPRequestPayload payload;
    payload.client_timestamp = (uint32_t)(millis() & 0xFFFFFFFF);

    uint8_t payloadBuf[64];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    Retcon::Service::ServiceMessage msg;
    msg.msg_type = Retcon::Service::MessageType::NTP_REQUEST;
    msg.service = "ntp";
    msg.payload.assign(payloadBuf, payloadLen);
    msg.request_id = payload.client_timestamp;

    _pending_ntp_request_id = msg.request_id;
    _last_ntp_request = millis();

    sendServiceMessage(serverHash, msg);
    Serial.printf("[Service] Sent NTP_REQUEST to %s\n", serverHash.toHex().substr(0, 12).c_str());
}

void RnsService::requestSearch(const RNS::Bytes& serverHash, const std::string& query, bool aiSummary) {
    Retcon::Service::SearchRequestPayload payload;
    payload.query = query;
    payload.max_results = 5;
    payload.ai_summary = aiSummary;

    uint8_t payloadBuf[256];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    uint32_t requestId = (uint32_t)(millis() & 0xFFFFFFFF);

    Retcon::Service::ServiceMessage msg;
    msg.msg_type = Retcon::Service::MessageType::SEARCH_REQUEST;
    msg.service = "search";
    msg.payload.assign(payloadBuf, payloadLen);
    msg.request_id = requestId;

    // Store pending search
    _pending_searches[requestId] = {query, requestId};

    sendServiceMessage(serverHash, msg);
    Serial.printf("[Service] Sent SEARCH_REQUEST for '%s' (AI: %s)\n", query.c_str(), aiSummary ? "yes" : "no");
}

// ============================================================================
// Maps Service Methods
// ============================================================================

void RnsService::requestMapTile(const RNS::Bytes& serverHash, uint8_t z, uint32_t x, uint32_t y,
                                 Retcon::Service::TileFormat format) {
    Retcon::Service::MapTileRequestPayload payload;
    payload.z = z;
    payload.x = x;
    payload.y = y;
    payload.format = format;

    uint8_t payloadBuf[64];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    uint32_t requestId = (uint32_t)(millis() & 0xFFFFFFFF);

    Retcon::Service::ServiceMessage msg;
    msg.msg_type = Retcon::Service::MessageType::MAP_TILE_REQUEST;
    msg.service = "maps";
    msg.payload.assign(payloadBuf, payloadLen);
    msg.request_id = requestId;

    // Store pending tile info for chunk reassembly
    _pending_tiles[requestId] = {z, x, y, format, 0, {}};

    sendServiceMessage(serverHash, msg);
    Serial.printf("[Service] Sent MAP_TILE_REQUEST z=%d x=%u y=%u\n", z, x, y);
}

void RnsService::requestRoute(const RNS::Bytes& serverHash, int32_t startLat, int32_t startLon,
                               int32_t endLat, int32_t endLon, Retcon::Service::TravelMode mode) {
    Retcon::Service::MapRouteRequestPayload payload;
    payload.start_lat = startLat;
    payload.start_lon = startLon;
    payload.end_lat = endLat;
    payload.end_lon = endLon;
    payload.mode = mode;

    uint8_t payloadBuf[64];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    uint32_t requestId = (uint32_t)(millis() & 0xFFFFFFFF);

    Retcon::Service::ServiceMessage msg;
    msg.msg_type = Retcon::Service::MessageType::MAP_ROUTE_REQUEST;
    msg.service = "maps";
    msg.payload.assign(payloadBuf, payloadLen);
    msg.request_id = requestId;

    _pending_routes[requestId] = {requestId};

    sendServiceMessage(serverHash, msg);
    Serial.printf("[Service] Sent MAP_ROUTE_REQUEST\n");
}

void RnsService::requestGeocode(const RNS::Bytes& serverHash, const std::string& query,
                                 int32_t biasLat, int32_t biasLon, bool hasBias, uint8_t maxResults) {
    Retcon::Service::MapGeocodeRequestPayload payload;
    payload.query = query;
    payload.bias_lat = biasLat;
    payload.bias_lon = biasLon;
    payload.has_bias = hasBias;
    payload.max_results = maxResults;

    uint8_t payloadBuf[256];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    uint32_t requestId = (uint32_t)(millis() & 0xFFFFFFFF);

    Retcon::Service::ServiceMessage msg;
    msg.msg_type = Retcon::Service::MessageType::MAP_GEOCODE_REQUEST;
    msg.service = "maps";
    msg.payload.assign(payloadBuf, payloadLen);
    msg.request_id = requestId;

    _pending_geocodes[requestId] = {query, requestId};

    sendServiceMessage(serverHash, msg);
    Serial.printf("[Service] Sent MAP_GEOCODE_REQUEST for '%s'\n", query.c_str());
}

void RnsService::handleServiceMessage(const Retcon::Service::ServiceMessage& msg, const RNS::Bytes& sourceHash) {
    Serial.printf("[Service] Handling message type 0x%02X from %s\n",
                  static_cast<uint8_t>(msg.msg_type), sourceHash.toHex().substr(0, 12).c_str());

    switch (msg.msg_type) {
        case Retcon::Service::MessageType::TRUST_OFFER: {
            Retcon::Service::TrustOfferPayload payload;
            payload.deserialize(msg.payload.data(), msg.payload.size());
            handleTrustOffer(payload, sourceHash);
            break;
        }

        case Retcon::Service::MessageType::NTP_RESPONSE: {
            Serial.printf("[Service] NTP_RESPONSE received from %s\n", sourceHash.toHex().c_str());

            // Only accept from trusted servers
            bool trusted = Retcon::Service::getTrustedServers().isTrusted(sourceHash);
            Serial.printf("[Service] Source trusted: %s\n", trusted ? "YES" : "NO");

            if (!trusted) {
                Serial.println("[Service] Ignoring NTP response from untrusted server");
                return;
            }

            Serial.printf("[Service] Deserializing NTP payload (%d bytes)\n", msg.payload.size());
            Retcon::Service::NTPResponsePayload payload;
            payload.deserialize(msg.payload.data(), msg.payload.size());
            Serial.printf("[Service] NTP payload: server_time=%u, client_time=%u\n",
                          payload.server_timestamp, payload.client_timestamp);
            handleNtpResponse(payload);
            break;
        }

        case Retcon::Service::MessageType::SEARCH_RESPONSE: {
            // Only accept from trusted servers
            if (!Retcon::Service::getTrustedServers().isTrusted(sourceHash)) {
                Serial.println("[Service] Ignoring search response from untrusted server");
                return;
            }
            Retcon::Service::SearchResponsePayload payload;
            payload.deserialize(msg.payload.data(), msg.payload.size());
            handleSearchResponse(payload, msg.request_id);
            break;
        }

        case Retcon::Service::MessageType::MAP_TILE_RESPONSE: {
            // Only accept from trusted servers
            if (!Retcon::Service::getTrustedServers().isTrusted(sourceHash)) {
                Serial.println("[Service] Ignoring tile response from untrusted server");
                return;
            }
            Retcon::Service::MapTileResponsePayload payload;
            payload.deserialize(msg.payload.data(), msg.payload.size());
            handleMapTileResponse(payload, msg.request_id);
            break;
        }

        case Retcon::Service::MessageType::MAP_ROUTE_RESPONSE: {
            // Only accept from trusted servers
            if (!Retcon::Service::getTrustedServers().isTrusted(sourceHash)) {
                Serial.println("[Service] Ignoring route response from untrusted server");
                return;
            }
            Retcon::Service::MapRouteResponsePayload payload;
            payload.deserialize(msg.payload.data(), msg.payload.size());
            handleRouteResponse(payload, msg.request_id);
            break;
        }

        case Retcon::Service::MessageType::MAP_GEOCODE_RESPONSE: {
            // Only accept from trusted servers
            if (!Retcon::Service::getTrustedServers().isTrusted(sourceHash)) {
                Serial.println("[Service] Ignoring geocode response from untrusted server");
                return;
            }
            Retcon::Service::MapGeocodeResponsePayload payload;
            payload.deserialize(msg.payload.data(), msg.payload.size());
            handleGeocodeResponse(payload, msg.request_id);
            break;
        }

        default:
            Serial.printf("[Service] Unknown message type: 0x%02X\n", static_cast<uint8_t>(msg.msg_type));
            break;
    }
}

void RnsService::handleTrustOffer(const Retcon::Service::TrustOfferPayload& payload, const RNS::Bytes& sourceHash) {
    Serial.printf("[Service] Received TRUST_OFFER from '%s' offering services:", payload.server_name.c_str());
    for (const auto& svc : payload.services) {
        Serial.printf(" %s", svc.c_str());
    }
    Serial.println();

    // Add to trusted servers as pending
    Retcon::Service::getTrustedServers().addPendingOffer(sourceHash, payload.server_name, payload.services);

    // Send event so UI can update
    Event e;
    e.src = this;
    e.type = EventType::SETTINGS_CHANGED;  // Reuse existing event type
    _retos->publishEvent(e);
}

void RnsService::handleNtpResponse(const Retcon::Service::NTPResponsePayload& payload) {
    // Delegate to TimeService for centralized time management
    TimeService* timeService = _retos->fetchService<TimeService>();
    if (timeService) {
        timeService->handleNtpResponse(payload.server_timestamp, payload.client_timestamp);
    } else {
        Serial.println("[Service] Warning: TimeService not found, cannot process NTP response");
    }
}

void RnsService::handleSearchResponse(const Retcon::Service::SearchResponsePayload& payload, uint32_t requestId) {
    Serial.printf("[Service] Received SEARCH_RESPONSE for '%s': %d results\n",
                  payload.query.c_str(), payload.results.size());

    if (!payload.error.empty()) {
        Serial.printf("[Service] Search error: %s\n", payload.error.c_str());
    }

    // Remove from pending
    _pending_searches.erase(requestId);

    // Send event with search results
    Event e;
    e.src = this;
    e.type = EventType::SEARCH_RESULTS;

    // Create a shared copy of results for the event
    auto results = std::make_shared<Retcon::Service::SearchResponsePayload>(payload);
    e.data = results;

    _retos->publishEvent(e);
}

// ============================================================================
// Maps Response Handlers
// ============================================================================

void RnsService::handleMapTileResponse(const Retcon::Service::MapTileResponsePayload& payload, uint32_t requestId) {
    Serial.printf("[Service] Received MAP_TILE_RESPONSE z=%d x=%u y=%u chunk %d/%d\n",
                  payload.z, payload.x, payload.y, payload.chunk_index + 1, payload.total_chunks);

    if (!payload.error.empty()) {
        Serial.printf("[Service] Tile error: %s\n", payload.error.c_str());
        _pending_tiles.erase(requestId);

        // Send error event
        Event e;
        e.src = this;
        e.type = EventType::MAP_TILE_RECEIVED;
        auto errorPayload = std::make_shared<Retcon::Service::MapTileResponsePayload>(payload);
        e.data = errorPayload;
        _retos->publishEvent(e);
        return;
    }

    // Find or create pending tile entry
    auto it = _pending_tiles.find(requestId);
    if (it == _pending_tiles.end()) {
        // First chunk - create entry
        _pending_tiles[requestId] = {payload.z, payload.x, payload.y, payload.format, payload.total_chunks, {}};
        it = _pending_tiles.find(requestId);
    }

    // Store this chunk
    it->second.chunks[payload.chunk_index] = payload.data;

    // Check if we have all chunks
    if (it->second.chunks.size() == payload.total_chunks) {
        // Reassemble tile data
        std::vector<uint8_t> fullData;
        for (uint16_t i = 0; i < payload.total_chunks; i++) {
            const auto& chunk = it->second.chunks[i];
            fullData.insert(fullData.end(), chunk.begin(), chunk.end());
        }

        // Create complete response payload
        Retcon::Service::MapTileResponsePayload completePayload;
        completePayload.z = it->second.z;
        completePayload.x = it->second.x;
        completePayload.y = it->second.y;
        completePayload.format = it->second.format;
        completePayload.chunk_index = 0;
        completePayload.total_chunks = 1;
        completePayload.data = fullData;

        // Remove from pending
        _pending_tiles.erase(requestId);

        // Send event with complete tile
        Event e;
        e.src = this;
        e.type = EventType::MAP_TILE_RECEIVED;
        auto results = std::make_shared<Retcon::Service::MapTileResponsePayload>(completePayload);
        e.data = results;
        _retos->publishEvent(e);

        Serial.printf("[Service] Tile complete: %zu bytes\n", fullData.size());
    }
}

void RnsService::handleRouteResponse(const Retcon::Service::MapRouteResponsePayload& payload, uint32_t requestId) {
    Serial.printf("[Service] Received MAP_ROUTE_RESPONSE: %zu points, %zu instructions\n",
                  payload.points.size() / 2, payload.instructions.size());

    if (!payload.error.empty()) {
        Serial.printf("[Service] Route error: %s\n", payload.error.c_str());
    }

    // Remove from pending
    _pending_routes.erase(requestId);

    // Send event with route
    Event e;
    e.src = this;
    e.type = EventType::MAP_ROUTE_RECEIVED;
    auto results = std::make_shared<Retcon::Service::MapRouteResponsePayload>(payload);
    e.data = results;
    _retos->publishEvent(e);
}

void RnsService::handleGeocodeResponse(const Retcon::Service::MapGeocodeResponsePayload& payload, uint32_t requestId) {
    Serial.printf("[Service] Received MAP_GEOCODE_RESPONSE for '%s': %zu results\n",
                  payload.query.c_str(), payload.results.size());

    if (!payload.error.empty()) {
        Serial.printf("[Service] Geocode error: %s\n", payload.error.c_str());
    }

    // Remove from pending
    _pending_geocodes.erase(requestId);

    // Send event with geocode results
    Event e;
    e.src = this;
    e.type = EventType::MAP_GEOCODE_RESULTS;
    auto results = std::make_shared<Retcon::Service::MapGeocodeResponsePayload>(payload);
    e.data = results;
    _retos->publishEvent(e);
}
