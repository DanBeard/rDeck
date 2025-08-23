#include "RDeckAnnounceHandler.h"
#include "LXMFData.h"

void RDeckAnnounceHandler::received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		INFO("ExampleAnnounceHandler: destination hash: " + destination_hash.toHex());
		if (announced_identity) {
			INFO("ExampleAnnounceHandler: announced identity hash: " + announced_identity.hash().toHex());
			INFO("ExampleAnnounceHandler: announced identity app data: " + announced_identity.app_data().toHex());
		}
        if (app_data) {
			INFO("ExampleAnnounceHandler: app data text: \"" + app_data.toString() + "\"");
		}
		time_t now;
    	time(&now);
		Retcon::LXMF::AnnounceData adata(destination_hash, app_data, now);
		announce_set->insert(adata);

		if(now - last_persist >= persist_delay_secs) {
			Retcon::LXMF::persistAnnounceData();
			last_persist = now;
		}
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}