#pragma once
#include "Transport.h"
class RDeckAnnounceHandler : public RNS::AnnounceHandler {
public:
	RDeckAnnounceHandler(const char* aspect_filter = nullptr) : AnnounceHandler(aspect_filter) {}
	virtual ~RDeckAnnounceHandler() {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		INFO("ExampleAnnounceHandler: destination hash: " + destination_hash.toHex());
		if (announced_identity) {
			INFO("ExampleAnnounceHandler: announced identity hash: " + announced_identity.hash().toHex());
			INFO("ExampleAnnounceHandler: announced identity app data: " + announced_identity.app_data().toHex());
		}
        if (app_data) {
			INFO("ExampleAnnounceHandler: app data text: \"" + app_data.toString() + "\"");
		}
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}
};
