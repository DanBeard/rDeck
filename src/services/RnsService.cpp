#include "RnsService.h"
#include "lvgl.h"
#include "RnsUtils/LoraInterface.h"

RnsService::RnsService(uint8_t id): reticulum({RNS::Type::NONE}),
 identity({RNS::Type::NONE}),
 destination({RNS::Type::NONE}),
  rns_fs({RNS::Type::NONE}),
  lora_interface({RNS::Type::NONE}),
   BaseService(id) {

}
void RnsService::start(RetOS* retos){
    updateIcon(false);

    _lora = retos->hal().lora;

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
    // new identity
    identity = RNS::Identity(true);

      // load identity
    //identity = RNS::Identity(false);
    //RNS::Bytes prv_bytes;
    //prv_bytes.assignHex("78E7D93E28D55871608FF13329A226CABC3903A357388A035B360162FF6321570B092E0583772AB80BC425F99791DF5CA2CA0A985FF0415DAB419BBC64DDFAE8");
    //identity.load_private_key(prv_bytes);

    destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "lxmf", "delivery");

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
}


void RnsService::announce() {
    if (destination) {
		HEAD("Announcing destination...", RNS::LOG_TRACE);
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
        char buffer[100];
        StaticJsonDocument<100> doc;
        JsonArray nameArray = doc.to<JsonArray>();
        nameArray.add("RDECK");
        nameArray.add(nullptr);
        size_t bytesWritten = serializeMsgPack(nameArray, buffer, 100);
        TRACE("LoRaInterface: announce bytes written = " + std::to_string(bytesWritten) + " ........");
		destination.announce(RNS::bytesFromChunk((const uint8_t*)buffer, bytesWritten), false, lora_interface);
	}
}

static unsigned long last_announce = 0;
void RnsService::tick() {
    // TODO TEMP FOR TESTING REMOVE ME OR MAKE MUCH LONGER OR VIA CONFIG
    unsigned long now = millis();
    if(now - last_announce > (10*1000)){
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