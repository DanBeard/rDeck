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
    //rnsService->lxmf_delivery_src.s
    Retcon::LXMF::Message lxmf_msg(plaintext);
    Retcon::LXMF::addMessageToConversation(lxmf_msg);

}
static void onLink(RNS::Link& link) {
    rnsService->reticulum.should_persist_data();
    Serial.println("LINK ESTABLISHED!");
    link.set_link_packet_callback(onLinkPacket);
}

void RnsService::start(RetOS* retos){
    updateIcon(false);

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
        .cr = 8,
        .power = 16,
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
    _announce_handler = make_shared<RDeckAnnounceHandler>();
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
    announce();
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

static unsigned long last_announce = 0;
void RnsService::tick(const time_t tMillis) {
    // TODO TEMP FOR TESTING REMOVE ME OR MAKE MUCH LONGER OR VIA CONFIG
    if(tMillis - last_announce > (10*60*1000)){
        Serial.println("RNS ANNOUNCE");
        announce();
        last_announce = tMillis;
        reticulum.should_persist_data();
    }
    reticulum.loop();
    lora_interface_impl->tick(lora_interface);
}

void RnsService::updateIcon(bool status){
    ServiceIcon iconInfo = {
        .serviceID = this->_id,
        .icon = status ? LV_SYMBOL_UPLOAD : "R?",
        .opacity = status ? LV_OPA_100 : LV_OPA_100,
    };
    _retos->ui()->setServiceIcon(iconInfo);
}



void RnsService::sendLxmfMsg(const RNS::Bytes dest, const string &title, const string &contents) {
    Retcon::LXMF::Message msg(lxmf_delivery_src.hash(), dest, title, contents);
    msg.status = Retcon::LXMF::Message::STATUS::QUEUEING;

    // if nothings going on, then just send it!
    if(!_sending_message && _send_msg_queue.size() == 0) {
        _sending_message = true;
        transmitMsg(msg);
    } else if(_send_msg_queue.size() > max_number_queued_msgs) {
        return; // drop it
    } else {
        _send_msg_queue.push(msg);
    }
    if(!_sending_message) {
        transmitMsg(_send_msg_queue.front());
        _send_msg_queue.pop();
    }

    // TODO queue and send message AND retry
}

const queue<Retcon::LXMF::Message>& RnsService::queuedMsgs() const {
    return _send_msg_queue;
}

void transmit_delivery_cb(const RNS::PacketReceipt &receipt) {
    rnsService->_sending_message = false;
    delete rnsService->_sending_packet;
    rnsService->_current_sending_msg.status = Retcon::LXMF::Message::STATUS::SENT;
    Retcon::LXMF::addMessageToConversation(rnsService->_current_sending_msg);

    if(rnsService->_send_msg_queue.size() > 0) {
        rnsService->transmitMsg(rnsService->_send_msg_queue.front());
        rnsService->_send_msg_queue.pop();
    }

}
void transmit_timeout_cb(const RNS::PacketReceipt &receipt) {
    rnsService->_current_sending_msg.status = Retcon::LXMF::Message::STATUS::RETRY;

    if(rnsService->_num_retries++ > RnsService::max_number_retries) {
        delete rnsService->_sending_packet;
        rnsService->_current_sending_msg.status = Retcon::LXMF::Message::STATUS::FAILED;
        Retcon::LXMF::addMessageToConversation(rnsService->_current_sending_msg);
        if(rnsService->_send_msg_queue.size() > 0) {
            rnsService->transmitMsg(rnsService->_send_msg_queue.front());
            rnsService->_send_msg_queue.pop();
        }
    }    
    
}
void RnsService::transmitMsg(const Retcon::LXMF::Message &msg) {
    if(_sending_message) return;
    _sending_message = true;
    _num_retries = 0;

    _current_sending_msg = msg;
    _current_sending_msg.status = Retcon::LXMF::Message::STATUS::SENDING;
    _sending_packet = new RNS::Packet(lxmf_delivery_src, _current_sending_msg.fullMsg());
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
            _settings.createNestedObject("lora");
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
