#include "RnsService.h"
#include "lvgl.h"
#include "RnsUtils/LoraInterface.h"
#include "Bytes.h"

// Yeah this means there can be only 1
static RnsService* rnsService = nullptr; 

RnsService::RnsService(uint8_t id): reticulum({RNS::Type::NONE}),
 identity({RNS::Type::NONE}),
 destination({RNS::Type::NONE}),
  rns_fs({RNS::Type::NONE}),
  lora_interface({RNS::Type::NONE}),
   BaseService(id) {
    // make sure there's only 1
    assert(rnsService == nullptr);
    rnsService = this;
}

// c style callbacks
static void onPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
    RNS::Bytes source = data.mid(0, 16);
    RNS::Bytes signature = data.mid(16, 16 + 64);
    RNS::Bytes packed_payload = data.mid(16 + 64);

    INFO("LXMF: source: " + source.toHex());
    INFO("LXMF: signature: " + signature.toHex());
    INFO("LXMF: packed_payload: " + packed_payload.toHex());

    // FIXME: validate signatures
    JsonDocument doc;
    deserializeMsgPack(doc,  packed_payload.data()); //packed_payload.size()
    float timestamp = doc[0];
    const char* title = doc[1];
    const char* contents = doc[2];
    //size_t fieldsize?

    Serial.println("[RNS] PACKET---------");
    Serial.println(timestamp);
    Serial.println(title);
    Serial.println(contents);
    Serial.println("[RNS] END PACKET---------");
}

static void onLink(RNS::Link& link) {
    Serial.println("LINK ESTABLISH? yay?");
}

void RnsService::start(RetOS* retos){
    updateIcon(false);

    RetHal hal = retos->hal();
    _lora = hal.lora;
    _fs = hal.fs;

    lora_interface_impl = new RNS::Interfaces::LoRaInterface(_lora);
    lora_interface = RNS::Interface(lora_interface_impl);
    rns_fs = new FileSystem();
    ((FileSystem*)rns_fs.get())->init();
    RNS::Utilities::OS::register_filesystem(rns_fs);

    lora_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
	RNS::Transport::register_interface(lora_interface);
    lora_interface_impl->start();

    reticulum = RNS::Reticulum();
    reticulum.transport_enabled(true);
	reticulum.start();

    // do we have a saved identity?
    if(!_fs->exists("/reticulum")){
        _fs->mkdir("/reticulum");
    }
    if(true || !_fs->exists("/reticulum/identity.priv")) {
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
    destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "lxmf", "delivery");
    destination.set_packet_callback(onPacket);
    destination.set_link_established_callback(onLink);
    destination.set_proof_strategy(RNS::Type::Destination::PROVE_NONE);


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
    if (destination) {
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
		destination.announce(RNS::bytesFromChunk(buffer, bytesWritten), false, lora_interface);
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
EventStatus RnsService::onEvent(Event& event){
     return IGNORED;
}