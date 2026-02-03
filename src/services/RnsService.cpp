#include "RnsService.h"
#include "lvgl.h"
#include "RnsUtils/LoraInterface.h"
#include "Bytes.h"
#include "apps/Settings.h"
#include "RnsUtils/LXMFData.h"

//using namespace Retcon::LXMF;

// Yeah this means there can be only 1
static RnsService* rnsService = nullptr; 

RnsService::RnsService(uint8_t id): reticulum({RNS::Type::NONE}),
 identity({RNS::Type::NONE}),
 lxmf_delivery_src({RNS::Type::NONE}),
  rns_fs({RNS::Type::NONE}),
  lora_interface({RNS::Type::NONE}),
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
    Serial.println("LINK ESTABLISHED!");
    link.set_packet_callback(onLinkPacket);
}

void RnsService::start(RetOS* retos){
    updateIcon(false);

    // PlatformMutex creates mutex in constructor, no explicit init needed

    // pause and then load config
    RetHal hal = retos->hal();
    _lora = hal.lora;
    _fs = hal.fs;

    lora_interface_impl = new RNS::Interfaces::LoRaInterface(_lora, this);
    lora_interface = RNS::Interface(lora_interface_impl);
    rns_fs = new FileSystem();
    ((FileSystem*)rns_fs.get())->init();
    RNS::Utilities::OS::register_filesystem(rns_fs);

    lora_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
	RNS::Transport::register_interface(lora_interface);

    // TODO merge with settings config
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
    // merge any changes the user has mad
    mergeLoraSettings(config);

    lora_interface_impl->start(config);

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
        const char* priv_hex = priv.toHex().c_str();

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

    // set to running
    _status = RUNNING;
    retos->run_later([this]() {
        Serial.println("RNS ANNOUNCE INITIAL");
        announce();
    }, 1500);
   
}

void RnsService::saveUserInfo() {
    File config_file = _fs->open("/reticulum/userinfo.json", FILE_WRITE, true);
    serializeJsonPretty(userInfo, config_file);
    config_file.close();
}

void RnsService::announce() {
    if (lxmf_delivery_src) {
		HEAD("Announcing destination...", RNS::LOG_TRACE);
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
        uint8_t buffer[200];
        JsonDocument doc;
        // throw together the name
        const char * name = userInfo["name"];
        doc.add(MsgPackBinary(name,strnlen(name,100)));
        //doc.add("RDECK STR");
        doc.add(nullptr);
        //doc.add("rand?");
        size_t bytesWritten = serializeMsgPack(doc, buffer, 200);
        Serial.println("ANNOUNCE MSGPACK-------");
        Serial.println(name);
        for(int i=0; i< bytesWritten; i++){
            Serial.print((int)buffer[i]); Serial.print(',');
        }
        Serial.println("\n END ANNOUNCE MSGPACK-------");
        TRACE("LoRaInterface: announce bytes written = " + std::to_string(bytesWritten) + " ........");
		lxmf_delivery_src.announce(RNS::bytesFromChunk(buffer, bytesWritten), false, lora_interface);
	}
}

void RnsService::processNextInQueue() {
    _msg_mutex.lock();
    if(!_sending_message && _send_msg_queue.size() > 0) {
        transmitMsg(_send_msg_queue.front());
        _send_msg_queue.pop();
    } else if(_sending_message && _current_sending_msg &&
              _current_sending_msg->status == Retcon::LXMF::Message::STATUS::RETRY) {
        // Retry the current message
        transmitMsg(_current_sending_msg);
    }
    _msg_mutex.unlock();
}

static unsigned long last_announce = 0;
void RnsService::tick(const unsigned long tMillis) {
    // TODO TEMP FOR TESTING REMOVE ME OR MAKE MUCH LONGER OR VIA CONFIG
    if(tMillis - last_announce > (10*60*1000) || tMillis < last_announce){
        Serial.println("RNS ANNOUNCE");
        announce();
        last_announce = tMillis;
        reticulum.should_persist_data();
    }
    reticulum.loop();
    lora_interface_impl->tick(lora_interface);

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

    _msg_mutex.lock();

    Serial.println("QUEUEING LXMF MESSAGE");
    if(_send_msg_queue.size() > max_number_queued_msgs) {
        Serial.println("DROPPING LXMF MESSAGE - queue full");
        _msg_mutex.unlock();
        return msg;
    }
    _send_msg_queue.push(msg);
    // Defer actual transmit to tick() on the services task, avoiding
    // cross-thread calls into Transport::outbound() which can deadlock.
    _needs_send_processing = true;

    _msg_mutex.unlock();

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
    rnsService->_msg_mutex.lock();

    rnsService->_sending_message = false;
    delete rnsService->_sending_packet;
    rnsService->_sending_packet = nullptr;
    rnsService->_current_sending_msg->status = Retcon::LXMF::Message::STATUS::SENT;
    Serial.println("[LXMF] Message delivery confirmed (ACK received)");
    rnsService->sendMessageUpdateEvent(rnsService->_current_sending_msg);

    // Defer sending next queued message to tick()
    rnsService->_needs_send_processing = true;

    rnsService->_msg_mutex.unlock();
}
void transmit_timeout_cb(const RNS::PacketReceipt &receipt) {
    rnsService->_msg_mutex.lock();

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

    rnsService->_msg_mutex.unlock();
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
