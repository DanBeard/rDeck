#include "ServiceProtocol.h"

using namespace Retcon::Service;

// Helper for safe string extraction from msgpack
static std::string safeGetString(JsonVariant var) {
    if (var.isNull()) return "";
    if (var.is<MsgPackBinary>()) {
        MsgPackBinary bin = var.as<MsgPackBinary>();
        return std::string((const char*)bin.data(), bin.size());
    }
    return var.as<std::string>();
}

// ============================================================================
// TrustOfferPayload
// ============================================================================

void TrustOfferPayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);

    server_name = safeGetString(doc["server_name"]);

    services.clear();
    if (doc["services"].is<JsonArray>()) {
        JsonArray arr = doc["services"];
        for (size_t i = 0; i < arr.size(); i++) {
            services.push_back(safeGetString(arr[i]));
        }
    }
}

size_t TrustOfferPayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["server_name"] = server_name;
    JsonArray arr = doc["services"].to<JsonArray>();
    for (const auto& svc : services) {
        arr.add(svc);
    }
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// TrustAcceptPayload
// ============================================================================

void TrustAcceptPayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);
    device_name = safeGetString(doc["device_name"]);
}

size_t TrustAcceptPayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["device_name"] = device_name;
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// NTPRequestPayload
// ============================================================================

void NTPRequestPayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);
    client_timestamp = doc["client_timestamp"] | 0;
}

size_t NTPRequestPayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["client_timestamp"] = client_timestamp;
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// NTPResponsePayload
// ============================================================================

void NTPResponsePayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);
    server_timestamp = doc["server_timestamp"] | 0;
    client_timestamp = doc["client_timestamp"] | 0;
}

size_t NTPResponsePayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["server_timestamp"] = server_timestamp;
    doc["client_timestamp"] = client_timestamp;
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// SearchRequestPayload
// ============================================================================

void SearchRequestPayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);
    query = safeGetString(doc["query"]);
    max_results = doc["max_results"] | 5;
}

size_t SearchRequestPayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["query"] = query;
    doc["max_results"] = max_results;
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// SearchResponsePayload
// ============================================================================

void SearchResponsePayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);

    query = safeGetString(doc["query"]);
    error = safeGetString(doc["error"]);

    results.clear();
    if (doc["results"].is<JsonArray>()) {
        JsonArray arr = doc["results"];
        for (size_t i = 0; i < arr.size(); i++) {
            SearchResult r;
            r.title = safeGetString(arr[i]["title"]);
            r.url = safeGetString(arr[i]["url"]);
            r.snippet = safeGetString(arr[i]["snippet"]);
            results.push_back(r);
        }
    }
}

size_t SearchResponsePayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["query"] = query;
    if (!error.empty()) {
        doc["error"] = error;
    }
    JsonArray arr = doc["results"].to<JsonArray>();
    for (const auto& r : results) {
        JsonObject obj = arr.add<JsonObject>();
        obj["title"] = r.title;
        obj["url"] = r.url;
        obj["snippet"] = r.snippet;
    }
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// ServiceMessage
// ============================================================================

bool ServiceMessage::isServiceMessage(JsonDocument& fields) {
    return fields.containsKey("msg_type") && fields.containsKey("service");
}

ServiceMessage ServiceMessage::fromFields(JsonDocument& fields) {
    ServiceMessage msg;
    msg.msg_type = static_cast<MessageType>(fields["msg_type"].as<uint8_t>());
    msg.service = safeGetString(fields["service"]);
    msg.request_id = fields["request_id"] | 0;

    // Extract payload bytes
    if (fields["payload"].is<MsgPackBinary>()) {
        MsgPackBinary bin = fields["payload"].as<MsgPackBinary>();
        msg.payload.assign((const uint8_t*)bin.data(), bin.size());
    }

    return msg;
}

void ServiceMessage::toFields(JsonDocument& fields) const {
    fields["msg_type"] = static_cast<uint8_t>(msg_type);
    fields["service"] = service;
    fields["request_id"] = request_id;
    if (payload.size() > 0) {
        fields["payload"] = MsgPackBinary(payload.data(), payload.size());
    }
}
