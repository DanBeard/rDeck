#pragma once
#include "Transport.h"
#include "retOS/retOS.h"
#include "sys/time.h"
#include "LXMFData.h"

class RDeckAnnounceHandler : public RNS::AnnounceHandler {

	set<Retcon::LXMF::AnnounceData> *announce_set;
	time_t last_persist = 0;
	static constexpr time_t persist_delay_secs = 60;
public:
	RDeckAnnounceHandler(const char* aspect_filter = nullptr) : AnnounceHandler(aspect_filter), announce_set(Retcon::LXMF::getAnnounceData()) {}
	virtual ~RDeckAnnounceHandler() {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data);
};
