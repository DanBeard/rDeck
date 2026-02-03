#pragma once
#include "BaseService.h"

#include <queue>
#include <map>
#include "retOS/PlatformMutex.h"
#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Transport.h"
#include "Interface.h"
#include "Log.h"
#include "Bytes.h"
#include "Type.h"
#include "Utilities/OS.h"
#include "RnsUtils/FileSystem.h"
#include "RnsUtils/LoraInterface.h"
#include "RnsUtils/RDeckAnnounceHandler.h"
#include "RnsUtils/LXMFData.h"
#include "RnsUtils/ServiceProtocol.h"
#include "RnsUtils/TrustedServers.h"



class RnsService: public BaseService {
    
    friend class LoraInterface;


public:

    static const size_t max_number_queued_msgs = 5;
    static const size_t max_number_retries = 4;
    static const time_t packet_timeout_secs = 10;

    explicit RnsService(uint8_t id);
    virtual void start(RetOS* retos) override; // called after construction once the OS is ready to launch services
    virtual void tick(const unsigned long tmillis) override; // called periodically by OS so you can do work. Time varies by sleep and power level but ~1-3ms while awake
    void announce();

    bool isValid = false;

    JsonDocument userInfo;
    void saveUserInfo();

    RNS::Destination lxmf_delivery_src;
    RNS::Reticulum reticulum;

    // send an LXmfMesssage. Pass in an updater function that wi;; be called when status changes with the message object
    typedef function<void(const shared_ptr<Retcon::LXMF::Message>&)> msg_update_cb;

    shared_ptr<Retcon::LXMF::Message> sendLxmfMsg(const RNS::Bytes dest, const string &title, const string &contents);
    const queue<shared_ptr<Retcon::LXMF::Message>>& queuedMsgs() const;

    // Service protocol methods
    void sendServiceMessage(const RNS::Bytes& dest, const Retcon::Service::ServiceMessage& msg);
    void sendTrustAccept(const RNS::Bytes& serverHash);
    void requestNtpSync(const RNS::Bytes& serverHash);
    void requestSearch(const RNS::Bytes& serverHash, const std::string& query);

    static constexpr const char* settingsSection = "reticulum";
    static bool drawSettings(lv_obj_t * column, Settings* settings);
    static void applySettings();
    static void mergeLoraSettings(LoraConfig& config);

    // Service message handling - called from packet callback
    void handleServiceMessage(const Retcon::Service::ServiceMessage& msg, const RNS::Bytes& sourceHash);

protected:
    void updateIcon(bool status);
    // just Lora for now, but we could do TCP/UDP/etc in the future over wifi
    BaseLora* _lora;
    FS* _fs;

    RNS::Identity identity;

    // our destination for sending/recing lxmf messages
    
    //RNS::Interfaces::UDPInterface udp_interface("udp");
    RNS::Interfaces::LoRaInterface *lora_interface_impl;
    RNS::Interface lora_interface;

    RNS::FileSystem rns_fs;

    std::shared_ptr<RDeckAnnounceHandler> _announce_handler;

    // Mutex to protect message state accessed from multiple tasks
    PlatformMutex _msg_mutex;

    boolean _sending_message = false;
    shared_ptr<Retcon::LXMF::Message> _current_sending_msg;
    RNS::Packet *_sending_packet = nullptr;
    std::queue<shared_ptr<Retcon::LXMF::Message>> _send_msg_queue;

    uint8_t _num_retries = 0;
    // actually do the tranmit
    void transmitMsg(shared_ptr<Retcon::LXMF::Message> &msg);
    void processNextInQueue();

    // Deferred retry/next-send flag — set from receipt callbacks (which run
    // inside Transport::jobs()), processed in tick() to avoid re-entering
    // Transport::outbound() while _jobs_running is still true.
    volatile bool _needs_send_processing = false;

    void sendMessageUpdateEvent(shared_ptr<Retcon::LXMF::Message> &msg);

    void handleTrustOffer(const Retcon::Service::TrustOfferPayload& payload, const RNS::Bytes& sourceHash);
    void handleNtpResponse(const Retcon::Service::NTPResponsePayload& payload);
    void handleSearchResponse(const Retcon::Service::SearchResponsePayload& payload, uint32_t requestId);

    // NTP sync state
    unsigned long _last_ntp_request = 0;
    uint32_t _pending_ntp_request_id = 0;
    static const unsigned long NTP_SYNC_INTERVAL = 30 * 60 * 1000;  // 30 minutes

    // Search state
    struct PendingSearch {
        std::string query;
        uint32_t request_id;
    };
    std::map<uint32_t, PendingSearch> _pending_searches;

    friend void transmit_delivery_cb(const RNS::PacketReceipt &receipt);
    friend void transmit_timeout_cb(const RNS::PacketReceipt &receipt);

};