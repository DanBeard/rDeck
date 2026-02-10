#pragma once
#include "BaseService.h"

#include <queue>
#include <map>
#include <functional>
#include <vector>
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
#include "RnsUtils/TCPClientInterface.h"
#include "RnsUtils/RDeckAnnounceHandler.h"
#include "RnsUtils/LXMFData.h"
#include "RnsUtils/ServiceProtocol.h"
#include "RnsUtils/TrustedServers.h"

class WifiService;



class RnsService: public BaseService {

    friend class LoraInterface;
    friend class TCPClientInterface;


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
    void requestSearch(const RNS::Bytes& serverHash, const std::string& query, bool aiSummary = false);

    // Thread-safe action queue: UI thread pushes lambdas, services thread drains in tick().
    // Use this for ALL calls from UI code into microReticulum (which is not thread-safe).
    void queueAction(std::function<void()> action);

    // Maps service methods
    void requestMapTile(const RNS::Bytes& serverHash, uint8_t z, uint32_t x, uint32_t y,
                        Retcon::Service::TileFormat format = Retcon::Service::TileFormat::MONO_RLE);
    void requestRoute(const RNS::Bytes& serverHash, int32_t startLat, int32_t startLon,
                      int32_t endLat, int32_t endLon,
                      Retcon::Service::TravelMode mode = Retcon::Service::TravelMode::WALK);
    void requestGeocode(const RNS::Bytes& serverHash, const std::string& query,
                        int32_t biasLat = 0, int32_t biasLon = 0, bool hasBias = false,
                        uint8_t maxResults = 5);

    static constexpr const char* settingsSection = "reticulum";
    static bool drawSettings(lv_obj_t * column, Settings* settings);
    static void applySettings();
    static void mergeLoraSettings(LoraConfig& config);

    // Service message handling - called from packet callback
    void handleServiceMessage(const Retcon::Service::ServiceMessage& msg, const RNS::Bytes& sourceHash);

    // Interface mode
    enum class InterfaceMode {
        LORA,
        TCP
    };
    InterfaceMode getInterfaceMode() const { return _interfaceMode; }
    bool isInterfaceOnline() const;

protected:
    void updateIcon(bool status);
    void initLoraInterface();
    void initTcpInterface();

    // Hardware/filesystem references
    BaseLora* _lora = nullptr;
    FS* _fs = nullptr;

    RNS::Identity identity;

    // Interface mode - either LoRa or TCP, mutually exclusive
    InterfaceMode _interfaceMode = InterfaceMode::LORA;

    // LoRa interface (used when WiFi mode disabled)
    RNS::Interfaces::LoRaInterface* lora_interface_impl = nullptr;
    RNS::Interface lora_interface;

    // TCP interface (used when WiFi mode enabled)
    RNS::Interfaces::TCPClientInterface* tcp_interface_impl = nullptr;
    RNS::Interface tcp_interface;

    RNS::FileSystem rns_fs;

    std::shared_ptr<RDeckAnnounceHandler> _announce_handler;

    PlatformMutex _msg_mutex;

    // Thread-safe action queue (UI thread → services thread)
    // Rate-limited to avoid overwhelming Transport with rapid-fire sends
    // (e.g., 9 tile requests queued at once from Maps).
    static const size_t MAX_ACTIONS_PER_TICK = 3;
    PlatformMutex _action_mutex;
    std::vector<std::function<void()>> _pending_actions;

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
    void handleMapTileResponse(const Retcon::Service::MapTileResponsePayload& payload, uint32_t requestId);
    void handleRouteResponse(const Retcon::Service::MapRouteResponsePayload& payload, uint32_t requestId);
    void handleGeocodeResponse(const Retcon::Service::MapGeocodeResponsePayload& payload, uint32_t requestId);

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

    // Maps state - tile chunk assembly
    struct PendingTile {
        uint8_t z;
        uint32_t x;
        uint32_t y;
        Retcon::Service::TileFormat format;
        uint16_t total_chunks;
        std::map<uint16_t, std::vector<uint8_t>> chunks;  // chunk_index -> data
    };
    std::map<uint32_t, PendingTile> _pending_tiles;  // request_id -> pending tile

    struct PendingRoute {
        uint32_t request_id;
    };
    std::map<uint32_t, PendingRoute> _pending_routes;

    struct PendingGeocode {
        std::string query;
        uint32_t request_id;
    };
    std::map<uint32_t, PendingGeocode> _pending_geocodes;

    friend void transmit_delivery_cb(const RNS::PacketReceipt &receipt);
    friend void transmit_timeout_cb(const RNS::PacketReceipt &receipt);

};