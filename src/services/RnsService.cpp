#include "RnsService.h"
#include "lvgl.h"
#include "RnsUtils/LoraInterface.h"
#include "Bytes.h"
#include "apps/Settings.h"

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

// c style callbacks
// static void onPacket(const RNS::Bytes& plaintext, const RNS::Packet& packet) {
//     RNS::Bytes dest = data.mid(0, 16);
//     RNS::Bytes source = data.mid(16, 16*2);
//     RNS::Bytes signature = data.mid(2*16, 2*16 + 64);
//     RNS::Bytes packed_payload = data.mid(2*16 + 64);

//     INFO("LXMF: source: " + source.toHex());
//     INFO("LXMF: signature: " + signature.toHex());
//     INFO("LXMF: packed_payload: " + packed_payload.toHex());

//     // FIXME: validate signatures
//     JsonDocument doc;
//     deserializeMsgPack(doc,  packed_payload.data()); //packed_payload.size()
//     float timestamp = doc[0];
//     const char* title = doc[1];
//     const char* contents = doc[2];
//     //size_t fieldsize?

//     Serial.println("[RNS] PACKET---------");
//     Serial.println(timestamp);
//     Serial.println(title);
//     Serial.println(contents);
//     Serial.println("[RNS] END PACKET---------");
// }
static void onLinkPacket(const RNS::Bytes& plaintext, const RNS::Packet& packet) {

    Serial.print("[[PACKET]] --");
    RNS::Bytes my_hash = plaintext.mid(0, 16);
    RNS::Bytes their_hash = plaintext.mid(16, 16);
    RNS::Bytes signature = plaintext.mid(2*16, 64);
    RNS::Bytes packed_payload = plaintext.mid(2*16 + 64);

    INFO("LXMF: plaintext: " + plaintext.toHex());
    INFO("LXMF: source: " + their_hash.toHex());
    INFO("LXMF: dest: " + my_hash.toHex());
    INFO("LXMF: signature: " + signature.toHex());
    INFO("LXMF: packed_payload: " + packed_payload.toHex());
    
    //Serial.println(plaintext.toHex().c_str());
    JsonDocument msg;
    deserializeMsgPack(msg, packed_payload.data(), packed_payload.size());
    // print to serial for debug
    serializeJsonPretty(msg, Serial);

    float timestamp = msg[0];
    MsgPackBinary title = msg[1];
    MsgPackBinary contents = msg[2];
    char text[200] = {0};
    memcpy(text, contents.data(), contents.size());
    Serial.println(" ");
    Serial.print(text);
    Serial.println("\n/[[PACKET]] --");

    RNS::Identity srcIdent = RNS::Identity::recall(their_hash);
  
    RNS::Destination srcDest(srcIdent, RNS::Type::Destination::OUT,RNS::Type::Destination::SINGLE, "lxmf",  "delivery");
    JsonDocument reply_payload;
    reply_payload.add(timestamp + 1);
    reply_payload.add(MsgPackBinary("ECHO", strlen("ECHO")));
    reply_payload.add(MsgPackBinary("ECHO", strlen("ECHO")));
    reply_payload.add(nullptr); //fields. dont support em for now

    uint8_t packed_reply_payload[300];
    size_t bytes_written = serializeMsgPack(reply_payload, packed_reply_payload, 300);

    RNS::Bytes hashed_part;
    hashed_part.append(their_hash);
    hashed_part.append(my_hash);
    hashed_part.append(packed_reply_payload, bytes_written); 

    RNS::Bytes hash = RNS::Identity::full_hash(hashed_part);
    RNS::Bytes signed_part;
    signed_part.append(hashed_part);
    signed_part.append(hash);

    RNS::Bytes reply_sig = rnsService->lxmf_delivery_src.sign(signed_part);
    Serial.println(their_hash.size());
    Serial.println(my_hash.size());
    Serial.println(reply_sig.size());
    RNS::Bytes packed;
    ///packed.append(their_hash);
    packed.append(my_hash);
    packed.append(reply_sig);
    packed.append(packed_reply_payload, bytes_written);
    RNS::Packet *send_packet = new RNS::Packet(srcDest, packed);
    const RNS::Link& link = *(packet.link());
    //send_packet.link((RNS::Link&)link);
    //send_packet.
    retOsGlobalPtr->run_later([send_packet]() {
        Serial.println("SENDING ECHO]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]");
        send_packet->send();
        delete send_packet;
        Serial.println("/SENDING ECHO]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]");
    }, 2500);
   
    //rnsService->lxmf_delivery_src.s



}
static void onLink(RNS::Link& link) {
    rnsService->reticulum.should_persist_data();
    Serial.println("LINK ESTABLISH? yay?");
    link.set_link_packet_callback(onLinkPacket);
}

void RnsService::start(RetOS* retos){
    updateIcon(false);

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
    lora_interface_impl->start();

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
void RnsService::tick() {
    // TODO TEMP FOR TESTING REMOVE ME OR MAKE MUCH LONGER OR VIA CONFIG
    unsigned long now = millis();
    if(now - last_announce > (10*60*1000)){
        Serial.println("RNS ANNOUNCE");
        announce();
        last_announce = now;
        reticulum.should_persist_data();
    }
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


static FunctorCallback settingsCallback;

bool RnsService::drawSettings(lv_obj_t * container, Settings* settings) {
    settings->drawSettingsSectionHeader(container, "Reticulum");

    JsonObject _settings = settings->getSettings(settingsSection);

    lv_obj_t *fr, *bd, *sf, *cr;

    // Create these BEFORE the functor so the pointers are valid when captured by functor
    fr = settings->drawSettingsTextInputRow(container, "Frequency", "0", &settingsCallback);
    bd = settings->drawSettingsTextInputRow(container, "Bandwidth", "0", &settingsCallback);
    sf = settings->drawSettingsTextInputRow(container, "SF", "0", &settingsCallback);
    cr = settings->drawSettingsTextInputRow(container, "CR", "0", &settingsCallback);

    // must be static so it survives past this function call.
    settingsCallback = [_settings, fr, bd, sf, cr](lv_event_t *e){
        lv_obj_t * ta = lv_event_get_target(e);
        // don't pass raw char* to JsonArduino or it won't copy them adn you'll get junk later
        String value = lv_textarea_get_text(ta);
        Serial.print("RNS change to =");
        Serial.println(value);
        if(ta == fr) {
            Serial.print("fr");
        } else if(ta == bd) {
             Serial.print("bd");
        } else if(ta == sf) {
             Serial.print("sf");
        } else if(ta == cr) {
             Serial.print("cr");
        } else {
            Serial.print("Unknown!!");
        }
        //_settings[timezone] = new_timezone;
        //_retos->time.setPosixTimezone(new_timezone.c_str());
    }; 

    return true;
}
