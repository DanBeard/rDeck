#include "TrustedServers.h"

using namespace Retcon::Service;

// Global singleton
static TrustedServers _trustedServersInstance;

TrustedServers& Retcon::Service::getTrustedServers() {
    return _trustedServersInstance;
}

void TrustedServer::serialize(JsonObject& obj) const {
    obj["name"] = name;
    obj["status"] = (status == TrustStatus::TRUSTED) ? "trusted" : "pending";
    obj["offered_at"] = (long)offered_at;
    obj["accepted_at"] = (long)accepted_at;

    JsonArray svcArr = obj["services"].to<JsonArray>();
    for (const auto& svc : services) {
        svcArr.add(svc);
    }
}

void TrustedServer::deserialize(JsonObject& obj) {
    if (obj.isNull()) return;

    name = obj["name"] | "";

    const char* statusStr = obj["status"] | "pending";
    status = (strcmp(statusStr, "trusted") == 0) ? TrustStatus::TRUSTED : TrustStatus::PENDING;

    offered_at = obj["offered_at"] | 0;
    accepted_at = obj["accepted_at"] | 0;

    services.clear();
    if (obj["services"].is<JsonArray>()) {
        JsonArray svcArr = obj["services"];
        for (size_t i = 0; i < svcArr.size(); i++) {
            const char* svc = svcArr[i];
            if (svc) services.push_back(svc);
        }
    }
}

void TrustedServers::init(FS* fs) {
    _fs = fs;
    load();
}

void TrustedServers::load() {
    if (_loaded || !_fs) return;

    if (_fs->exists(STORAGE_FILE)) {
        File file = _fs->open(STORAGE_FILE, FILE_READ);
        if (file) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();

            if (error == DeserializationError::Ok) {
                int version = doc["version"] | 0;
                if (version == SCHEMA_VERSION && doc["servers"].is<JsonObject>()) {
                    JsonObject servers = doc["servers"];
                    for (JsonPair kv : servers) {
                        const char* hashHex = kv.key().c_str();
                        JsonObject serverObj = kv.value().as<JsonObject>();

                        TrustedServer server;
                        server.hash.assignHex(hashHex);
                        server.deserialize(serverObj);

                        _servers[hashHex] = server;
                    }
                    Serial.printf("[TrustedServers] Loaded %d servers\n", _servers.size());
                } else {
                    Serial.println("[TrustedServers] Version mismatch, starting fresh");
                }
            } else {
                Serial.printf("[TrustedServers] Parse error: %s\n", error.c_str());
            }
        }
    }

    _loaded = true;
}

void TrustedServers::persist() {
    if (!_fs) return;

    JsonDocument doc;
    doc["version"] = SCHEMA_VERSION;
    JsonObject servers = doc["servers"].to<JsonObject>();

    for (const auto& kv : _servers) {
        JsonObject serverObj = servers[kv.first].to<JsonObject>();
        kv.second.serialize(serverObj);
    }

    File file = _fs->open(STORAGE_FILE, FILE_WRITE, true);
    if (file) {
        serializeJsonPretty(doc, file);
        file.close();
        Serial.printf("[TrustedServers] Persisted %d servers\n", _servers.size());
    } else {
        Serial.println("[TrustedServers] Failed to open file for writing");
    }
}

void TrustedServers::addPendingOffer(const RNS::Bytes& hash, const std::string& name,
                                      const std::vector<std::string>& services) {
    std::string hashHex = hash.toHex();

    // Check if we already have this server trusted - don't overwrite
    auto it = _servers.find(hashHex);
    if (it != _servers.end() && it->second.status == TrustStatus::TRUSTED) {
        Serial.println("[TrustedServers] Ignoring offer from already trusted server");
        return;
    }

    TrustedServer server;
    server.hash = hash;
    server.name = name;
    server.services = services;
    server.status = TrustStatus::PENDING;
    time(&server.offered_at);

    _servers[hashHex] = server;
    persist();

    Serial.printf("[TrustedServers] Added pending offer from '%s'\n", name.c_str());
}

bool TrustedServers::acceptOffer(const std::string& hashHex) {
    auto it = _servers.find(hashHex);
    if (it == _servers.end()) {
        Serial.println("[TrustedServers] Cannot accept - no offer found");
        return false;
    }

    it->second.status = TrustStatus::TRUSTED;
    time(&it->second.accepted_at);
    persist();

    Serial.printf("[TrustedServers] Accepted trust from '%s'\n", it->second.name.c_str());
    return true;
}

void TrustedServers::revokeTrust(const std::string& hashHex) {
    auto it = _servers.find(hashHex);
    if (it != _servers.end()) {
        Serial.printf("[TrustedServers] Revoked trust for '%s'\n", it->second.name.c_str());
        _servers.erase(it);
        persist();
    }
}

bool TrustedServers::isTrusted(const std::string& hashHex) const {
    auto it = _servers.find(hashHex);
    return it != _servers.end() && it->second.status == TrustStatus::TRUSTED;
}

bool TrustedServers::isTrusted(const RNS::Bytes& hash) const {
    return isTrusted(hash.toHex());
}

bool TrustedServers::hasPendingOffer(const std::string& hashHex) const {
    auto it = _servers.find(hashHex);
    return it != _servers.end() && it->second.status == TrustStatus::PENDING;
}

const TrustedServer* TrustedServers::getServer(const std::string& hashHex) const {
    auto it = _servers.find(hashHex);
    if (it != _servers.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<TrustedServer> TrustedServers::getAllServers() const {
    std::vector<TrustedServer> result;
    for (const auto& kv : _servers) {
        result.push_back(kv.second);
    }
    return result;
}

std::vector<TrustedServer> TrustedServers::getPendingOffers() const {
    std::vector<TrustedServer> result;
    for (const auto& kv : _servers) {
        if (kv.second.status == TrustStatus::PENDING) {
            result.push_back(kv.second);
        }
    }
    return result;
}

std::vector<TrustedServer> TrustedServers::getTrustedServers() const {
    std::vector<TrustedServer> result;
    for (const auto& kv : _servers) {
        if (kv.second.status == TrustStatus::TRUSTED) {
            result.push_back(kv.second);
        }
    }
    return result;
}
