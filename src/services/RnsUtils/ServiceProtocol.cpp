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
    ai_summary = doc["ai_summary"] | false;
}

size_t SearchRequestPayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["query"] = query;
    doc["max_results"] = max_results;
    doc["ai_summary"] = ai_summary;
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
    summary = safeGetString(doc["summary"]);

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
    if (!summary.empty()) {
        doc["summary"] = summary;
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
// MapTileRequestPayload
// ============================================================================

void MapTileRequestPayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);
    z = doc["z"] | 0;
    x = doc["x"] | 0;
    y = doc["y"] | 0;
    format = static_cast<TileFormat>(doc["format"] | 0);
}

size_t MapTileRequestPayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["z"] = z;
    doc["x"] = x;
    doc["y"] = y;
    doc["format"] = static_cast<uint8_t>(format);
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// MapTileResponsePayload
// ============================================================================

void MapTileResponsePayload::deserialize(const uint8_t* dataPtr, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, dataPtr, len);
    z = doc["z"] | 0;
    x = doc["x"] | 0;
    y = doc["y"] | 0;
    format = static_cast<TileFormat>(doc["format"] | 0);
    error = safeGetString(doc["error"]);

    data.clear();
    if (doc["data"].is<MsgPackBinary>()) {
        MsgPackBinary bin = doc["data"].as<MsgPackBinary>();
        const uint8_t* binData = static_cast<const uint8_t*>(bin.data());
        data.assign(binData, binData + bin.size());
    }
}

size_t MapTileResponsePayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["z"] = z;
    doc["x"] = x;
    doc["y"] = y;
    doc["format"] = static_cast<uint8_t>(format);
    if (!error.empty()) {
        doc["error"] = error;
    }
    if (!data.empty()) {
        doc["data"] = MsgPackBinary(data.data(), data.size());
    }
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// MapRouteRequestPayload
// ============================================================================

void MapRouteRequestPayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);
    start_lat = doc["start_lat"] | 0;
    start_lon = doc["start_lon"] | 0;
    end_lat = doc["end_lat"] | 0;
    end_lon = doc["end_lon"] | 0;
    mode = static_cast<TravelMode>(doc["mode"] | 0);
}

size_t MapRouteRequestPayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["start_lat"] = start_lat;
    doc["start_lon"] = start_lon;
    doc["end_lat"] = end_lat;
    doc["end_lon"] = end_lon;
    doc["mode"] = static_cast<uint8_t>(mode);
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// MapRouteResponsePayload
// ============================================================================

void MapRouteResponsePayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);

    total_distance_m = doc["total_distance_m"] | 0;
    total_time_s = doc["total_time_s"] | 0;
    error = safeGetString(doc["error"]);

    points.clear();
    if (doc["points"].is<JsonArray>()) {
        JsonArray arr = doc["points"];
        for (size_t i = 0; i < arr.size(); i++) {
            points.push_back(arr[i] | 0);
        }
    }

    instructions.clear();
    if (doc["instructions"].is<JsonArray>()) {
        JsonArray arr = doc["instructions"];
        for (size_t i = 0; i < arr.size(); i++) {
            MapRouteInstruction inst;
            inst.distance_m = arr[i]["distance_m"] | 0;
            inst.maneuver = safeGetString(arr[i]["maneuver"]);
            inst.street = safeGetString(arr[i]["street"]);
            inst.bearing = arr[i]["bearing"] | 0;
            instructions.push_back(inst);
        }
    }
}

size_t MapRouteResponsePayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["total_distance_m"] = total_distance_m;
    doc["total_time_s"] = total_time_s;
    if (!error.empty()) {
        doc["error"] = error;
    }

    JsonArray ptsArr = doc["points"].to<JsonArray>();
    for (int32_t pt : points) {
        ptsArr.add(pt);
    }

    JsonArray instArr = doc["instructions"].to<JsonArray>();
    for (const auto& inst : instructions) {
        JsonObject obj = instArr.add<JsonObject>();
        obj["distance_m"] = inst.distance_m;
        obj["maneuver"] = inst.maneuver;
        obj["street"] = inst.street;
        obj["bearing"] = inst.bearing;
    }

    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// MapGeocodeRequestPayload
// ============================================================================

void MapGeocodeRequestPayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);
    query = safeGetString(doc["query"]);
    max_results = doc["max_results"] | 5;

    if (doc.containsKey("bias_lat") && doc.containsKey("bias_lon")) {
        bias_lat = doc["bias_lat"] | 0;
        bias_lon = doc["bias_lon"] | 0;
        has_bias = true;
    } else {
        has_bias = false;
    }
}

size_t MapGeocodeRequestPayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["query"] = query;
    doc["max_results"] = max_results;
    if (has_bias) {
        doc["bias_lat"] = bias_lat;
        doc["bias_lon"] = bias_lon;
    }
    return serializeMsgPack(doc, buffer, maxLen);
}

// ============================================================================
// MapGeocodeResponsePayload
// ============================================================================

void MapGeocodeResponsePayload::deserialize(const uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeMsgPack(doc, data, len);

    query = safeGetString(doc["query"]);
    error = safeGetString(doc["error"]);

    results.clear();
    if (doc["results"].is<JsonArray>()) {
        JsonArray arr = doc["results"];
        for (size_t i = 0; i < arr.size(); i++) {
            MapGeocodeResult r;
            r.display_name = safeGetString(arr[i]["display_name"]);
            r.lat = arr[i]["lat"] | 0;
            r.lon = arr[i]["lon"] | 0;
            r.type = safeGetString(arr[i]["type"]);
            results.push_back(r);
        }
    }
}

size_t MapGeocodeResponsePayload::serialize(uint8_t* buffer, size_t maxLen) const {
    JsonDocument doc;
    doc["query"] = query;
    if (!error.empty()) {
        doc["error"] = error;
    }

    JsonArray arr = doc["results"].to<JsonArray>();
    for (const auto& r : results) {
        JsonObject obj = arr.add<JsonObject>();
        obj["display_name"] = r.display_name;
        obj["lat"] = r.lat;
        obj["lon"] = r.lon;
        obj["type"] = r.type;
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
