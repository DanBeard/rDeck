#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <string>
#include <vector>
#include <map>
#include "Bytes.h"

/**
 * Trusted Servers persistence for companion server trust relationships
 *
 * Storage file: /trusted_servers.json
 * {
 *   "version": 1,
 *   "servers": {
 *     "<hash>": {
 *       "name": "Home Server",
 *       "services": ["ntp", "search"],
 *       "status": "pending" | "trusted",
 *       "offered_at": 1706825600
 *     }
 *   }
 * }
 */

namespace Retcon::Service {

// Trust status
enum class TrustStatus : uint8_t {
    PENDING = 0,   // Server sent offer, waiting for user to accept
    TRUSTED = 1,   // User accepted, mutual trust established
};

/**
 * Information about a trusted server
 */
struct TrustedServer {
    RNS::Bytes hash;                 // Server destination hash
    std::string name;                // Server display name
    std::vector<std::string> services;  // Services offered
    TrustStatus status = TrustStatus::PENDING;
    time_t offered_at = 0;           // When offer was received
    time_t accepted_at = 0;          // When we accepted (0 if pending)

    void serialize(JsonObject& obj) const;
    void deserialize(JsonObject& obj);

    std::string hashHex() const { return hash.toHex(); }
};

/**
 * Manager for trusted server relationships
 */
class TrustedServers {
public:
    static constexpr const char* STORAGE_FILE = "/trusted_servers.json";
    static constexpr int SCHEMA_VERSION = 1;

    /**
     * Initialize and load from storage
     */
    void init(FS* fs);

    /**
     * Add a pending trust offer from a server
     */
    void addPendingOffer(const RNS::Bytes& hash, const std::string& name,
                         const std::vector<std::string>& services);

    /**
     * Accept a pending trust offer
     */
    bool acceptOffer(const std::string& hashHex);

    /**
     * Revoke trust for a server
     */
    void revokeTrust(const std::string& hashHex);

    /**
     * Check if a server is mutually trusted
     */
    bool isTrusted(const std::string& hashHex) const;
    bool isTrusted(const RNS::Bytes& hash) const;

    /**
     * Check if we have a pending offer from a server
     */
    bool hasPendingOffer(const std::string& hashHex) const;

    /**
     * Get a specific server
     */
    const TrustedServer* getServer(const std::string& hashHex) const;

    /**
     * Get all servers (for UI display)
     */
    std::vector<TrustedServer> getAllServers() const;

    /**
     * Get pending offers (for Settings display)
     */
    std::vector<TrustedServer> getPendingOffers() const;

    /**
     * Get trusted servers
     */
    std::vector<TrustedServer> getTrustedServers() const;

    /**
     * Persist to storage
     */
    void persist();

private:
    void load();

    FS* _fs = nullptr;
    std::map<std::string, TrustedServer> _servers;  // Key: hash hex string
    bool _loaded = false;
};

// Global singleton instance
TrustedServers& getTrustedServers();

} // namespace Retcon::Service
