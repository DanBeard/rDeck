/**
 * Unit tests for ServiceProtocol message serialization/deserialization.
 *
 * These tests verify that the C++ implementation can correctly serialize
 * and deserialize service messages, and that the encoding matches the
 * Python companion server for cross-platform compatibility.
 */

#include <unity.h>
#include <ArduinoJson.h>
#include <string>
#include "services/RnsUtils/ServiceProtocol.h"
#include "services/RnsUtils/TrustedServers.h"
#include "FS.h"

using namespace Retcon::Service;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Message Type Enum Tests - Must match Python values exactly
// ============================================================================

void test_message_type_trust_offer_value(void) {
    TEST_ASSERT_EQUAL(0x01, static_cast<uint8_t>(MessageType::TRUST_OFFER));
}

void test_message_type_trust_accept_value(void) {
    TEST_ASSERT_EQUAL(0x02, static_cast<uint8_t>(MessageType::TRUST_ACCEPT));
}

void test_message_type_trust_revoke_value(void) {
    TEST_ASSERT_EQUAL(0x03, static_cast<uint8_t>(MessageType::TRUST_REVOKE));
}

void test_message_type_ntp_request_value(void) {
    TEST_ASSERT_EQUAL(0x10, static_cast<uint8_t>(MessageType::NTP_REQUEST));
}

void test_message_type_ntp_response_value(void) {
    TEST_ASSERT_EQUAL(0x11, static_cast<uint8_t>(MessageType::NTP_RESPONSE));
}

void test_message_type_search_request_value(void) {
    TEST_ASSERT_EQUAL(0x20, static_cast<uint8_t>(MessageType::SEARCH_REQUEST));
}

void test_message_type_search_response_value(void) {
    TEST_ASSERT_EQUAL(0x21, static_cast<uint8_t>(MessageType::SEARCH_RESPONSE));
}

// Maps message type values
void test_message_type_map_tile_request_value(void) {
    TEST_ASSERT_EQUAL(0x30, static_cast<uint8_t>(MessageType::MAP_TILE_REQUEST));
}

void test_message_type_map_tile_response_value(void) {
    TEST_ASSERT_EQUAL(0x31, static_cast<uint8_t>(MessageType::MAP_TILE_RESPONSE));
}

void test_message_type_map_route_request_value(void) {
    TEST_ASSERT_EQUAL(0x33, static_cast<uint8_t>(MessageType::MAP_ROUTE_REQUEST));
}

void test_message_type_map_route_response_value(void) {
    TEST_ASSERT_EQUAL(0x34, static_cast<uint8_t>(MessageType::MAP_ROUTE_RESPONSE));
}

void test_message_type_map_geocode_request_value(void) {
    TEST_ASSERT_EQUAL(0x35, static_cast<uint8_t>(MessageType::MAP_GEOCODE_REQUEST));
}

void test_message_type_map_geocode_response_value(void) {
    TEST_ASSERT_EQUAL(0x36, static_cast<uint8_t>(MessageType::MAP_GEOCODE_RESPONSE));
}

// ============================================================================
// TrustOfferPayload Tests
// ============================================================================

void test_trust_offer_serialize_deserialize(void) {
    TrustOfferPayload original;
    original.server_name = "Test Server";
    original.services.push_back("ntp");
    original.services.push_back("search");

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(len > 0);

    TrustOfferPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("Test Server", decoded.server_name.c_str());
    TEST_ASSERT_EQUAL(2, decoded.services.size());
    TEST_ASSERT_EQUAL_STRING("ntp", decoded.services[0].c_str());
    TEST_ASSERT_EQUAL_STRING("search", decoded.services[1].c_str());
}

void test_trust_offer_empty_services(void) {
    TrustOfferPayload original;
    original.server_name = "Server";
    // No services

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    TrustOfferPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(0, decoded.services.size());
}

void test_trust_offer_canonical_structure(void) {
    TrustOfferPayload payload;
    payload.server_name = "TestServer";
    payload.services.push_back("ntp");

    uint8_t buffer[256];
    size_t len = payload.serialize(buffer, sizeof(buffer));

    // Verify msgpack structure by deserializing with ArduinoJson
    JsonDocument doc;
    deserializeMsgPack(doc, buffer, len);

    TEST_ASSERT_EQUAL_STRING("TestServer", doc["server_name"].as<const char*>());
    TEST_ASSERT_TRUE(doc["services"].is<JsonArray>());
    TEST_ASSERT_EQUAL(1, doc["services"].size());
}

// ============================================================================
// TrustAcceptPayload Tests
// ============================================================================

void test_trust_accept_serialize_deserialize(void) {
    TrustAcceptPayload original;
    original.device_name = "My rDeck";

    uint8_t buffer[128];
    size_t len = original.serialize(buffer, sizeof(buffer));

    TrustAcceptPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("My rDeck", decoded.device_name.c_str());
}

void test_trust_accept_canonical_structure(void) {
    TrustAcceptPayload payload;
    payload.device_name = "rDeck123";

    uint8_t buffer[128];
    size_t len = payload.serialize(buffer, sizeof(buffer));

    JsonDocument doc;
    deserializeMsgPack(doc, buffer, len);

    TEST_ASSERT_EQUAL_STRING("rDeck123", doc["device_name"].as<const char*>());
}

// ============================================================================
// NTPRequestPayload Tests
// ============================================================================

void test_ntp_request_serialize_deserialize(void) {
    NTPRequestPayload original;
    original.client_timestamp = 1234567890;

    uint8_t buffer[64];
    size_t len = original.serialize(buffer, sizeof(buffer));

    NTPRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(1234567890, decoded.client_timestamp);
}

void test_ntp_request_canonical_structure(void) {
    NTPRequestPayload payload;
    payload.client_timestamp = 12345;

    uint8_t buffer[64];
    size_t len = payload.serialize(buffer, sizeof(buffer));

    JsonDocument doc;
    deserializeMsgPack(doc, buffer, len);

    TEST_ASSERT_EQUAL(12345, doc["client_timestamp"].as<uint32_t>());
}

// ============================================================================
// NTPResponsePayload Tests
// ============================================================================

void test_ntp_response_serialize_deserialize(void) {
    NTPResponsePayload original;
    original.server_timestamp = 1706825600;
    original.client_timestamp = 1234567890;

    uint8_t buffer[64];
    size_t len = original.serialize(buffer, sizeof(buffer));

    NTPResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(1706825600, decoded.server_timestamp);
    TEST_ASSERT_EQUAL(1234567890, decoded.client_timestamp);
}

void test_ntp_response_canonical_structure(void) {
    NTPResponsePayload payload;
    payload.server_timestamp = 99999;
    payload.client_timestamp = 88888;

    uint8_t buffer[64];
    size_t len = payload.serialize(buffer, sizeof(buffer));

    JsonDocument doc;
    deserializeMsgPack(doc, buffer, len);

    TEST_ASSERT_EQUAL(99999, doc["server_timestamp"].as<uint32_t>());
    TEST_ASSERT_EQUAL(88888, doc["client_timestamp"].as<uint32_t>());
}

// ============================================================================
// SearchRequestPayload Tests
// ============================================================================

void test_search_request_serialize_deserialize(void) {
    SearchRequestPayload original;
    original.query = "test query";
    original.max_results = 3;

    uint8_t buffer[128];
    size_t len = original.serialize(buffer, sizeof(buffer));

    SearchRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("test query", decoded.query.c_str());
    TEST_ASSERT_EQUAL(3, decoded.max_results);
}

void test_search_request_default_max_results(void) {
    SearchRequestPayload payload;
    payload.query = "hello";
    // max_results not set, should default to 5

    TEST_ASSERT_EQUAL(5, payload.max_results);
}

// ============================================================================
// SearchResponsePayload Tests
// ============================================================================

void test_search_response_with_results(void) {
    SearchResponsePayload original;
    original.query = "python";
    original.results.push_back({"Python.org", "https://python.org", "Official site"});
    original.results.push_back({"Learn Python", "https://learn.python.org", "Tutorial"});

    uint8_t buffer[1024];
    size_t len = original.serialize(buffer, sizeof(buffer));

    SearchResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("python", decoded.query.c_str());
    TEST_ASSERT_EQUAL(2, decoded.results.size());
    TEST_ASSERT_EQUAL_STRING("Python.org", decoded.results[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("https://python.org", decoded.results[0].url.c_str());
    TEST_ASSERT_EQUAL_STRING("Tutorial", decoded.results[1].snippet.c_str());
    TEST_ASSERT_TRUE(decoded.error.empty());
}

void test_search_response_with_error(void) {
    SearchResponsePayload original;
    original.query = "test";
    original.error = "Network error";

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    SearchResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("test", decoded.query.c_str());
    TEST_ASSERT_EQUAL(0, decoded.results.size());
    TEST_ASSERT_EQUAL_STRING("Network error", decoded.error.c_str());
}

void test_search_response_empty_results(void) {
    SearchResponsePayload original;
    original.query = "obscure query";
    // No results, no error

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    SearchResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(0, decoded.results.size());
}

// ============================================================================
// MapTileRequestPayload Tests
// ============================================================================

void test_map_tile_request_serialize_deserialize(void) {
    MapTileRequestPayload original;
    original.z = 14;
    original.x = 2746;
    original.y = 6327;
    original.format = TileFormat::MONO_RLE;

    uint8_t buffer[64];
    size_t len = original.serialize(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(len > 0);

    MapTileRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(14, decoded.z);
    TEST_ASSERT_EQUAL(2746, decoded.x);
    TEST_ASSERT_EQUAL(6327, decoded.y);
    TEST_ASSERT_EQUAL(TileFormat::MONO_RLE, decoded.format);
}

void test_map_tile_request_canonical_structure(void) {
    MapTileRequestPayload payload;
    payload.z = 12;
    payload.x = 1000;
    payload.y = 2000;
    payload.format = TileFormat::RAW_1BIT;

    uint8_t buffer[64];
    size_t len = payload.serialize(buffer, sizeof(buffer));

    JsonDocument doc;
    deserializeMsgPack(doc, buffer, len);

    TEST_ASSERT_EQUAL(12, doc["z"].as<uint8_t>());
    TEST_ASSERT_EQUAL(1000, doc["x"].as<uint32_t>());
    TEST_ASSERT_EQUAL(2000, doc["y"].as<uint32_t>());
    TEST_ASSERT_EQUAL(1, doc["format"].as<uint8_t>());  // RAW_1BIT = 1
}

// ============================================================================
// MapTileResponsePayload Tests
// ============================================================================

void test_map_tile_response_serialize_deserialize(void) {
    MapTileResponsePayload original;
    original.z = 14;
    original.x = 2746;
    original.y = 6327;
    original.format = TileFormat::MONO_RLE;
    original.data = {0x01, 0xFF, 0x02, 0x00, 0x03, 0xAA};  // Sample RLE data

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapTileResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(14, decoded.z);
    TEST_ASSERT_EQUAL(2746, decoded.x);
    TEST_ASSERT_EQUAL(6327, decoded.y);
    TEST_ASSERT_EQUAL(TileFormat::MONO_RLE, decoded.format);
    TEST_ASSERT_EQUAL(6, decoded.data.size());
    TEST_ASSERT_EQUAL(0x01, decoded.data[0]);
    TEST_ASSERT_EQUAL(0xAA, decoded.data[5]);
}

void test_map_tile_response_with_error(void) {
    MapTileResponsePayload original;
    original.z = 14;
    original.x = 9999;
    original.y = 9999;
    original.format = TileFormat::MONO_RLE;
    original.error = "Tile not found";

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapTileResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("Tile not found", decoded.error.c_str());
    TEST_ASSERT_EQUAL(0, decoded.data.size());
}

// ============================================================================
// MapRouteRequestPayload Tests
// ============================================================================

void test_map_route_request_serialize_deserialize(void) {
    MapRouteRequestPayload original;
    original.start_lat = 377749000;  // 37.7749 * 1e7
    original.start_lon = -1224194000;  // -122.4194 * 1e7
    original.end_lat = 377850000;
    original.end_lon = -1224000000;
    original.mode = TravelMode::BIKE;

    uint8_t buffer[64];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapRouteRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(377749000, decoded.start_lat);
    TEST_ASSERT_EQUAL(-1224194000, decoded.start_lon);
    TEST_ASSERT_EQUAL(377850000, decoded.end_lat);
    TEST_ASSERT_EQUAL(-1224000000, decoded.end_lon);
    TEST_ASSERT_EQUAL(TravelMode::BIKE, decoded.mode);
}

void test_map_route_request_canonical_structure(void) {
    MapRouteRequestPayload payload;
    payload.start_lat = 100000000;
    payload.start_lon = 200000000;
    payload.end_lat = 300000000;
    payload.end_lon = 400000000;
    payload.mode = TravelMode::CAR;

    uint8_t buffer[64];
    size_t len = payload.serialize(buffer, sizeof(buffer));

    JsonDocument doc;
    deserializeMsgPack(doc, buffer, len);

    TEST_ASSERT_EQUAL(100000000, doc["start_lat"].as<int32_t>());
    TEST_ASSERT_EQUAL(200000000, doc["start_lon"].as<int32_t>());
    TEST_ASSERT_EQUAL(2, doc["mode"].as<uint8_t>());  // CAR = 2
}

// ============================================================================
// MapRouteResponsePayload Tests
// ============================================================================

void test_map_route_response_serialize_deserialize(void) {
    MapRouteResponsePayload original;
    original.points = {377749000, -1224194000, 377750000, -1224190000, 377760000, -1224180000};
    original.instructions.push_back({100, "start", "Main St"});
    original.instructions.push_back({200, "turn-left", "Oak Ave"});
    original.instructions.push_back({50, "arrive", ""});
    original.total_distance_m = 350;
    original.total_time_s = 240;

    uint8_t buffer[512];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapRouteResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(6, decoded.points.size());
    TEST_ASSERT_EQUAL(377749000, decoded.points[0]);
    TEST_ASSERT_EQUAL(-1224180000, decoded.points[5]);
    TEST_ASSERT_EQUAL(3, decoded.instructions.size());
    TEST_ASSERT_EQUAL(100, decoded.instructions[0].distance_m);
    TEST_ASSERT_EQUAL_STRING("start", decoded.instructions[0].maneuver.c_str());
    TEST_ASSERT_EQUAL_STRING("Main St", decoded.instructions[0].street.c_str());
    TEST_ASSERT_EQUAL_STRING("arrive", decoded.instructions[2].maneuver.c_str());
    TEST_ASSERT_EQUAL(350, decoded.total_distance_m);
    TEST_ASSERT_EQUAL(240, decoded.total_time_s);
}

void test_map_route_response_with_error(void) {
    MapRouteResponsePayload original;
    original.error = "No route found";

    uint8_t buffer[128];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapRouteResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("No route found", decoded.error.c_str());
    TEST_ASSERT_EQUAL(0, decoded.points.size());
}

// ============================================================================
// MapGeocodeRequestPayload Tests
// ============================================================================

void test_map_geocode_request_serialize_deserialize(void) {
    MapGeocodeRequestPayload original;
    original.query = "1600 Amphitheatre Parkway";
    original.bias_lat = 377749000;
    original.bias_lon = -1224194000;
    original.has_bias = true;
    original.max_results = 3;

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapGeocodeRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("1600 Amphitheatre Parkway", decoded.query.c_str());
    TEST_ASSERT_EQUAL(377749000, decoded.bias_lat);
    TEST_ASSERT_EQUAL(-1224194000, decoded.bias_lon);
    TEST_ASSERT_TRUE(decoded.has_bias);
    TEST_ASSERT_EQUAL(3, decoded.max_results);
}

void test_map_geocode_request_without_bias(void) {
    MapGeocodeRequestPayload original;
    original.query = "Empire State Building";
    original.has_bias = false;
    original.max_results = 5;

    uint8_t buffer[128];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapGeocodeRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("Empire State Building", decoded.query.c_str());
    TEST_ASSERT_FALSE(decoded.has_bias);
}

// ============================================================================
// MapGeocodeResponsePayload Tests
// ============================================================================

void test_map_geocode_response_serialize_deserialize(void) {
    MapGeocodeResponsePayload original;
    original.query = "coffee";
    original.results.push_back({"Starbucks, 123 Main St, City", 377749000, -1224194000, "cafe"});
    original.results.push_back({"Peet's Coffee, 456 Oak Ave", 377750000, -1224190000, "cafe"});

    uint8_t buffer[512];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapGeocodeResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("coffee", decoded.query.c_str());
    TEST_ASSERT_EQUAL(2, decoded.results.size());
    TEST_ASSERT_EQUAL_STRING("Starbucks, 123 Main St, City", decoded.results[0].display_name.c_str());
    TEST_ASSERT_EQUAL(377749000, decoded.results[0].lat);
    TEST_ASSERT_EQUAL(-1224194000, decoded.results[0].lon);
    TEST_ASSERT_EQUAL_STRING("cafe", decoded.results[0].type.c_str());
}

void test_map_geocode_response_with_error(void) {
    MapGeocodeResponsePayload original;
    original.query = "xyzabc123";
    original.error = "Geocoding service unavailable";

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapGeocodeResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL_STRING("xyzabc123", decoded.query.c_str());
    TEST_ASSERT_EQUAL_STRING("Geocoding service unavailable", decoded.error.c_str());
    TEST_ASSERT_EQUAL(0, decoded.results.size());
}

void test_map_geocode_response_empty_results(void) {
    MapGeocodeResponsePayload original;
    original.query = "nonexistent place";
    // No results, no error

    uint8_t buffer[128];
    size_t len = original.serialize(buffer, sizeof(buffer));

    MapGeocodeResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(0, decoded.results.size());
    TEST_ASSERT_TRUE(decoded.error.empty());
}

// ============================================================================
// Propagation Message Type Values
// ============================================================================

void test_message_type_prop_sync_request_value(void) {
    TEST_ASSERT_EQUAL(0x40, static_cast<uint8_t>(MessageType::PROP_SYNC_REQUEST));
}

void test_message_type_prop_sync_response_value(void) {
    TEST_ASSERT_EQUAL(0x41, static_cast<uint8_t>(MessageType::PROP_SYNC_RESPONSE));
}

void test_message_type_prop_msg_deliver_value(void) {
    TEST_ASSERT_EQUAL(0x42, static_cast<uint8_t>(MessageType::PROP_MSG_DELIVER));
}

void test_message_type_prop_submit_request_value(void) {
    TEST_ASSERT_EQUAL(0x43, static_cast<uint8_t>(MessageType::PROP_SUBMIT_REQUEST));
}

void test_message_type_prop_submit_response_value(void) {
    TEST_ASSERT_EQUAL(0x44, static_cast<uint8_t>(MessageType::PROP_SUBMIT_RESPONSE));
}

// ============================================================================
// PropSyncRequestPayload Tests
// ============================================================================

void test_prop_sync_request_serialize_deserialize(void) {
    PropSyncRequestPayload original;
    original.lxmf_dest_hash.assignHex("abcdef0123456789abcdef0123456789");
    RNS::Bytes id1;
    id1.assignHex("1111111111111111111111111111111111111111111111111111111111111111");
    original.known_ids.push_back(id1);
    original.max_messages = 5;

    uint8_t buffer[512];
    size_t len = original.serialize(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(len > 0);

    PropSyncRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(16, decoded.lxmf_dest_hash.size());
    TEST_ASSERT_EQUAL_STRING("abcdef0123456789abcdef0123456789", decoded.lxmf_dest_hash.toHex().c_str());
    TEST_ASSERT_EQUAL(1, decoded.known_ids.size());
    TEST_ASSERT_EQUAL(5, decoded.max_messages);
}

void test_prop_sync_request_empty_known_ids(void) {
    PropSyncRequestPayload original;
    original.lxmf_dest_hash.assignHex("abcdef0123456789abcdef0123456789");
    // No known_ids

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    PropSyncRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(0, decoded.known_ids.size());
    TEST_ASSERT_EQUAL(10, decoded.max_messages);  // default
}

// ============================================================================
// PropSyncResponsePayload Tests
// ============================================================================

void test_prop_sync_response_serialize_deserialize(void) {
    PropSyncResponsePayload original;
    original.count = 3;

    uint8_t buffer[128];
    size_t len = original.serialize(buffer, sizeof(buffer));

    PropSyncResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(3, decoded.count);
    TEST_ASSERT_TRUE(decoded.error.empty());
}

void test_prop_sync_response_with_error(void) {
    PropSyncResponsePayload original;
    original.count = 0;
    original.error = "Propagation not enabled";

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    PropSyncResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(0, decoded.count);
    TEST_ASSERT_EQUAL_STRING("Propagation not enabled", decoded.error.c_str());
}

// ============================================================================
// PropMsgDeliverPayload Tests
// ============================================================================

void test_prop_msg_deliver_serialize_deserialize(void) {
    PropMsgDeliverPayload original;
    original.transient_id.assignHex("aabbccdd11223344aabbccdd11223344aabbccdd11223344aabbccdd11223344");
    // Simulate raw LXMF: 16 bytes dest + 16 bytes src + 64 bytes sig + some payload
    uint8_t fakeMsg[100];
    for (int i = 0; i < 100; i++) fakeMsg[i] = (uint8_t)(i & 0xFF);
    original.raw_lxmf.assign(fakeMsg, 100);

    uint8_t buffer[512];
    size_t len = original.serialize(buffer, sizeof(buffer));

    PropMsgDeliverPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(32, decoded.transient_id.size());
    TEST_ASSERT_EQUAL(100, decoded.raw_lxmf.size());
    TEST_ASSERT_EQUAL(0, decoded.raw_lxmf.data()[0]);
    TEST_ASSERT_EQUAL(99, decoded.raw_lxmf.data()[99]);
}

// ============================================================================
// PropSubmitRequestPayload Tests
// ============================================================================

void test_prop_submit_request_serialize_deserialize(void) {
    PropSubmitRequestPayload original;
    uint8_t fakeMsg[200];
    for (int i = 0; i < 200; i++) fakeMsg[i] = (uint8_t)(i & 0xFF);
    original.raw_lxmf.assign(fakeMsg, 200);

    uint8_t buffer[512];
    size_t len = original.serialize(buffer, sizeof(buffer));

    PropSubmitRequestPayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_EQUAL(200, decoded.raw_lxmf.size());
    TEST_ASSERT_EQUAL(0, decoded.raw_lxmf.data()[0]);
    TEST_ASSERT_EQUAL(199, decoded.raw_lxmf.data()[199]);
}

// ============================================================================
// PropSubmitResponsePayload Tests
// ============================================================================

void test_prop_submit_response_accepted(void) {
    PropSubmitResponsePayload original;
    original.accepted = true;
    original.transient_id.assignHex("aabbccdd11223344aabbccdd11223344aabbccdd11223344aabbccdd11223344");

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    PropSubmitResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_TRUE(decoded.accepted);
    TEST_ASSERT_EQUAL(32, decoded.transient_id.size());
    TEST_ASSERT_TRUE(decoded.error.empty());
}

void test_prop_submit_response_rejected(void) {
    PropSubmitResponsePayload original;
    original.accepted = false;
    original.error = "Propagation store full";

    uint8_t buffer[256];
    size_t len = original.serialize(buffer, sizeof(buffer));

    PropSubmitResponsePayload decoded;
    decoded.deserialize(buffer, len);

    TEST_ASSERT_FALSE(decoded.accepted);
    TEST_ASSERT_EQUAL_STRING("Propagation store full", decoded.error.c_str());
}

// ============================================================================
// ServiceMessage Tests
// ============================================================================

void test_service_message_is_service_message_true(void) {
    JsonDocument fields;
    fields["msg_type"] = 0x01;
    fields["service"] = "trust";

    TEST_ASSERT_TRUE(ServiceMessage::isServiceMessage(fields));
}

void test_service_message_is_service_message_false(void) {
    JsonDocument fields;
    fields["some_field"] = "value";

    TEST_ASSERT_FALSE(ServiceMessage::isServiceMessage(fields));
}

void test_service_message_from_fields(void) {
    // Prepare payload
    TrustAcceptPayload payload;
    payload.device_name = "TestDevice";

    uint8_t payloadBuf[128];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    // Create fields
    JsonDocument fields;
    fields["msg_type"] = 0x02;
    fields["service"] = "trust";
    fields["request_id"] = 42;
    fields["payload"] = MsgPackBinary(payloadBuf, payloadLen);

    ServiceMessage msg = ServiceMessage::fromFields(fields);

    TEST_ASSERT_EQUAL(MessageType::TRUST_ACCEPT, msg.msg_type);
    TEST_ASSERT_EQUAL_STRING("trust", msg.service.c_str());
    TEST_ASSERT_EQUAL(42, msg.request_id);
    TEST_ASSERT_TRUE(msg.payload.size() > 0);
}

void test_service_message_to_fields(void) {
    ServiceMessage msg;
    msg.msg_type = MessageType::NTP_REQUEST;
    msg.service = "ntp";
    msg.request_id = 100;

    NTPRequestPayload payload;
    payload.client_timestamp = 9999;
    uint8_t buf[64];
    size_t len = payload.serialize(buf, sizeof(buf));
    msg.payload.assign(buf, len);

    JsonDocument fields;
    msg.toFields(fields);

    TEST_ASSERT_EQUAL(0x10, fields["msg_type"].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("ntp", fields["service"].as<const char*>());
    TEST_ASSERT_EQUAL(100, fields["request_id"].as<uint32_t>());
}

// ============================================================================
// Cross-Compatibility Tests - Canonical formats matching Python
// ============================================================================

void test_cross_compat_trust_offer_encoding(void) {
    // This test creates data that must be decodable by Python
    TrustOfferPayload payload;
    payload.server_name = "TestServer";
    payload.services.push_back("ntp");
    payload.services.push_back("search");

    uint8_t buffer[256];
    size_t len = payload.serialize(buffer, sizeof(buffer));

    // Verify the structure matches what Python expects
    JsonDocument doc;
    deserializeMsgPack(doc, buffer, len);

    // Python expects: {"server_name": "...", "services": [...]}
    TEST_ASSERT_TRUE(doc.containsKey("server_name"));
    TEST_ASSERT_TRUE(doc.containsKey("services"));
    TEST_ASSERT_EQUAL_STRING("TestServer", doc["server_name"].as<const char*>());
    TEST_ASSERT_EQUAL(2, doc["services"].size());
}

void test_cross_compat_ntp_response_encoding(void) {
    NTPResponsePayload payload;
    payload.server_timestamp = 1706825600;
    payload.client_timestamp = 1000;

    uint8_t buffer[64];
    size_t len = payload.serialize(buffer, sizeof(buffer));

    JsonDocument doc;
    deserializeMsgPack(doc, buffer, len);

    // Python expects: {"server_timestamp": ..., "client_timestamp": ...}
    TEST_ASSERT_TRUE(doc.containsKey("server_timestamp"));
    TEST_ASSERT_TRUE(doc.containsKey("client_timestamp"));
    TEST_ASSERT_EQUAL(1706825600, doc["server_timestamp"].as<uint32_t>());
    TEST_ASSERT_EQUAL(1000, doc["client_timestamp"].as<uint32_t>());
}

void test_cross_compat_full_message_structure(void) {
    ServiceMessage msg;
    msg.msg_type = MessageType::TRUST_ACCEPT;
    msg.service = "trust";
    msg.request_id = 12345;

    TrustAcceptPayload payload;
    payload.device_name = "rDeck";
    uint8_t buf[64];
    size_t len = payload.serialize(buf, sizeof(buf));
    msg.payload.assign(buf, len);

    JsonDocument fields;
    msg.toFields(fields);

    // Python expects these exact field names
    TEST_ASSERT_TRUE(fields.containsKey("msg_type"));
    TEST_ASSERT_TRUE(fields.containsKey("service"));
    TEST_ASSERT_TRUE(fields.containsKey("payload"));
    TEST_ASSERT_TRUE(fields.containsKey("request_id"));

    TEST_ASSERT_EQUAL(0x02, fields["msg_type"].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("trust", fields["service"].as<const char*>());
    TEST_ASSERT_EQUAL(12345, fields["request_id"].as<uint32_t>());
}

// ============================================================================
// LXMF Message Flow Simulation Tests
// These tests simulate receiving an LXMF message exactly as Python would send it
// ============================================================================

/**
 * Simulate what Python's LXMF library produces when sending a service message.
 *
 * Python code:
 *   fields = encode_service_fields(msg)  # {msg_type, service, payload, request_id}
 *   lxm = LXMF.LXMessage(..., fields=fields)
 *
 * LXMF packs this as: [timestamp, title_bytes, content_bytes, fields_dict]
 */
void test_lxmf_payload_with_service_fields(void) {
    // Step 1: Create the inner payload (what goes in fields["payload"])
    TrustOfferPayload innerPayload;
    innerPayload.server_name = "TestCompanionServer";
    innerPayload.services.push_back("ntp");
    innerPayload.services.push_back("search");

    uint8_t innerPayloadBuf[256];
    size_t innerPayloadLen = innerPayload.serialize(innerPayloadBuf, sizeof(innerPayloadBuf));

    // Step 2: Create the fields dict (what Python puts in LXMF message)
    JsonDocument fieldsDoc;
    fieldsDoc["msg_type"] = 0x01;  // TRUST_OFFER
    fieldsDoc["service"] = "trust";
    fieldsDoc["request_id"] = 12345;
    fieldsDoc["payload"] = MsgPackBinary(innerPayloadBuf, innerPayloadLen);

    // Step 3: Create the full LXMF packed payload: [timestamp, title, content, fields]
    JsonDocument lxmfPayload;
    JsonArray arr = lxmfPayload.to<JsonArray>();
    arr.add(1706825600);  // timestamp
    arr.add(MsgPackBinary((const uint8_t*)"", 0));  // empty title
    arr.add(MsgPackBinary((const uint8_t*)"", 0));  // empty content
    arr.add(fieldsDoc);  // fields dict

    // Serialize to msgpack (simulating wire format)
    uint8_t lxmfBuf[512];
    size_t lxmfLen = serializeMsgPack(lxmfPayload, lxmfBuf, sizeof(lxmfBuf));
    TEST_ASSERT_TRUE(lxmfLen > 0);

    // Step 4: Now simulate C++ receiving this - deserialize and extract fields
    JsonDocument receivedDoc;
    DeserializationError err = deserializeMsgPack(receivedDoc, lxmfBuf, lxmfLen);
    TEST_ASSERT_TRUE(err == DeserializationError::Ok);

    // Verify it's an array with at least 4 elements
    TEST_ASSERT_TRUE(receivedDoc.is<JsonArray>());
    TEST_ASSERT_TRUE(receivedDoc.size() >= 4);

    // Extract fields (index 3)
    JsonVariant fieldsVar = receivedDoc[3];
    TEST_ASSERT_FALSE(fieldsVar.isNull());
    TEST_ASSERT_TRUE(fieldsVar.is<JsonObject>());

    // Copy to a JsonDocument for isServiceMessage check
    JsonDocument fields;
    fields.set(fieldsVar);

    // Step 5: Verify service message detection works
    TEST_ASSERT_TRUE(ServiceMessage::isServiceMessage(fields));

    // Step 6: Parse the service message
    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::TRUST_OFFER, svcMsg.msg_type);
    TEST_ASSERT_EQUAL_STRING("trust", svcMsg.service.c_str());
    TEST_ASSERT_EQUAL(12345, svcMsg.request_id);
    TEST_ASSERT_TRUE(svcMsg.payload.size() > 0);

    // Step 7: Deserialize the inner payload
    TrustOfferPayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());
    TEST_ASSERT_EQUAL_STRING("TestCompanionServer", decoded.server_name.c_str());
    TEST_ASSERT_EQUAL(2, decoded.services.size());
    TEST_ASSERT_EQUAL_STRING("ntp", decoded.services[0].c_str());
    TEST_ASSERT_EQUAL_STRING("search", decoded.services[1].c_str());
}

/**
 * Test that we correctly detect when fields is NOT a service message.
 * This simulates receiving a regular LXMF message (not from companion server).
 */
void test_lxmf_payload_without_service_fields(void) {
    // Regular LXMF message: [timestamp, title, content, null or empty fields]
    JsonDocument lxmfPayload;
    JsonArray arr = lxmfPayload.to<JsonArray>();
    arr.add(1706825600);
    arr.add(MsgPackBinary((const uint8_t*)"Hello", 5));
    arr.add(MsgPackBinary((const uint8_t*)"Message body", 12));
    arr.add(nullptr);  // No fields

    uint8_t buf[256];
    size_t len = serializeMsgPack(lxmfPayload, buf, sizeof(buf));

    JsonDocument receivedDoc;
    deserializeMsgPack(receivedDoc, buf, len);

    TEST_ASSERT_TRUE(receivedDoc.is<JsonArray>());
    TEST_ASSERT_TRUE(receivedDoc.size() >= 4);

    JsonVariant fieldsVar = receivedDoc[3];
    // Fields should be null for regular messages
    TEST_ASSERT_TRUE(fieldsVar.isNull());
}

/**
 * Test LXMF message with empty fields dict (not a service message).
 */
void test_lxmf_payload_with_empty_fields(void) {
    JsonDocument lxmfPayload;
    JsonArray arr = lxmfPayload.to<JsonArray>();
    arr.add(1706825600);
    arr.add(MsgPackBinary((const uint8_t*)"Title", 5));
    arr.add(MsgPackBinary((const uint8_t*)"Content", 7));

    JsonDocument emptyFields;
    emptyFields.to<JsonObject>();  // Empty object {}
    arr.add(emptyFields);

    uint8_t buf[256];
    size_t len = serializeMsgPack(lxmfPayload, buf, sizeof(buf));

    JsonDocument receivedDoc;
    deserializeMsgPack(receivedDoc, buf, len);

    JsonVariant fieldsVar = receivedDoc[3];
    TEST_ASSERT_FALSE(fieldsVar.isNull());
    TEST_ASSERT_TRUE(fieldsVar.is<JsonObject>());

    JsonDocument fields;
    fields.set(fieldsVar);

    // Should NOT be detected as service message (missing msg_type and service)
    TEST_ASSERT_FALSE(ServiceMessage::isServiceMessage(fields));
}

/**
 * Test decoding NTP response as Python would send it.
 */
void test_lxmf_ntp_response_flow(void) {
    // Inner payload
    NTPResponsePayload innerPayload;
    innerPayload.server_timestamp = 1706825600;
    innerPayload.client_timestamp = 5000;

    uint8_t innerBuf[64];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    // Service message fields
    JsonDocument fieldsDoc;
    fieldsDoc["msg_type"] = 0x11;  // NTP_RESPONSE
    fieldsDoc["service"] = "ntp";
    fieldsDoc["request_id"] = 42;
    fieldsDoc["payload"] = MsgPackBinary(innerBuf, innerLen);

    // Full LXMF payload
    JsonDocument lxmfPayload;
    JsonArray arr = lxmfPayload.to<JsonArray>();
    arr.add(1706825600);
    arr.add(MsgPackBinary((const uint8_t*)"", 0));
    arr.add(MsgPackBinary((const uint8_t*)"", 0));
    arr.add(fieldsDoc);

    uint8_t buf[256];
    size_t len = serializeMsgPack(lxmfPayload, buf, sizeof(buf));

    // Simulate receiving
    JsonDocument receivedDoc;
    deserializeMsgPack(receivedDoc, buf, len);

    JsonDocument fields;
    fields.set(receivedDoc[3]);

    TEST_ASSERT_TRUE(ServiceMessage::isServiceMessage(fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::NTP_RESPONSE, svcMsg.msg_type);
    TEST_ASSERT_EQUAL(42, svcMsg.request_id);

    NTPResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());
    TEST_ASSERT_EQUAL(1706825600, decoded.server_timestamp);
    TEST_ASSERT_EQUAL(5000, decoded.client_timestamp);
}

/**
 * Test decoding search response with results as Python would send it.
 */
void test_lxmf_search_response_flow(void) {
    // Inner payload
    SearchResponsePayload innerPayload;
    innerPayload.query = "test query";
    innerPayload.results.push_back({"Result 1", "https://example.com/1", "First result"});
    innerPayload.results.push_back({"Result 2", "https://example.com/2", "Second result"});

    uint8_t innerBuf[512];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    // Service message fields
    JsonDocument fieldsDoc;
    fieldsDoc["msg_type"] = 0x21;  // SEARCH_RESPONSE
    fieldsDoc["service"] = "search";
    fieldsDoc["request_id"] = 999;
    fieldsDoc["payload"] = MsgPackBinary(innerBuf, innerLen);

    // Full LXMF payload
    JsonDocument lxmfPayload;
    JsonArray arr = lxmfPayload.to<JsonArray>();
    arr.add(1706825600);
    arr.add(MsgPackBinary((const uint8_t*)"", 0));
    arr.add(MsgPackBinary((const uint8_t*)"", 0));
    arr.add(fieldsDoc);

    uint8_t buf[1024];
    size_t len = serializeMsgPack(lxmfPayload, buf, sizeof(buf));

    // Simulate receiving
    JsonDocument receivedDoc;
    deserializeMsgPack(receivedDoc, buf, len);

    JsonDocument fields;
    fields.set(receivedDoc[3]);

    TEST_ASSERT_TRUE(ServiceMessage::isServiceMessage(fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::SEARCH_RESPONSE, svcMsg.msg_type);

    SearchResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());
    TEST_ASSERT_EQUAL_STRING("test query", decoded.query.c_str());
    TEST_ASSERT_EQUAL(2, decoded.results.size());
    TEST_ASSERT_EQUAL_STRING("Result 1", decoded.results[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("https://example.com/2", decoded.results[1].url.c_str());
}

/**
 * Test that fields with only msg_type (missing service) is not detected as service message.
 */
void test_partial_fields_not_service_message(void) {
    JsonDocument fields;
    fields["msg_type"] = 0x01;
    // Missing "service" field

    TEST_ASSERT_FALSE(ServiceMessage::isServiceMessage(fields));
}

/**
 * Test that fields with only service (missing msg_type) is not detected as service message.
 */
void test_partial_fields_missing_msg_type(void) {
    JsonDocument fields;
    fields["service"] = "trust";
    // Missing "msg_type" field

    TEST_ASSERT_FALSE(ServiceMessage::isServiceMessage(fields));
}

// ============================================================================
// Full Integration Test: LXMF → ServiceMessage → TrustedServers
// ============================================================================

/**
 * Test the complete flow from receiving an LXMF trust offer to storing in TrustedServers.
 * This simulates exactly what happens in RnsService::onLinkPacket → handleServiceMessage → handleTrustOffer
 */
void test_full_trust_offer_integration(void) {
    // Setup: Create test filesystem and TrustedServers instance
    char dirTemplate[] = "/tmp/test_integration_XXXXXX";
    char* testDir = mkdtemp(dirTemplate);
    TEST_ASSERT_NOT_NULL(testDir);

    FS testFs(testDir);
    TrustedServers servers;
    servers.init(&testFs);

    // Verify empty initially
    TEST_ASSERT_EQUAL(0, servers.getPendingOffers().size());

    // Step 1: Simulate Python server creating TRUST_OFFER
    TrustOfferPayload innerPayload;
    innerPayload.server_name = "HomeCompanionServer";
    innerPayload.services.push_back("ntp");
    innerPayload.services.push_back("search");

    uint8_t innerBuf[256];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    // Step 2: Create LXMF fields (as Python does)
    JsonDocument fieldsDoc;
    fieldsDoc["msg_type"] = static_cast<uint8_t>(MessageType::TRUST_OFFER);
    fieldsDoc["service"] = "trust";
    fieldsDoc["request_id"] = 99999;
    fieldsDoc["payload"] = MsgPackBinary(innerBuf, innerLen);

    // Step 3: Create full LXMF packed payload
    JsonDocument lxmfPayload;
    JsonArray arr = lxmfPayload.to<JsonArray>();
    arr.add(1706825600);
    arr.add(MsgPackBinary((const uint8_t*)"", 0));
    arr.add(MsgPackBinary((const uint8_t*)"", 0));
    arr.add(fieldsDoc);

    uint8_t lxmfBuf[512];
    size_t lxmfLen = serializeMsgPack(lxmfPayload, lxmfBuf, sizeof(lxmfBuf));

    // Step 4: Simulate RnsService receiving this (onLinkPacket logic)
    JsonDocument receivedDoc;
    deserializeMsgPack(receivedDoc, lxmfBuf, lxmfLen);

    TEST_ASSERT_TRUE(receivedDoc.is<JsonArray>());
    TEST_ASSERT_TRUE(receivedDoc.size() >= 4);

    JsonVariant fieldsVar = receivedDoc[3];
    TEST_ASSERT_FALSE(fieldsVar.isNull());
    TEST_ASSERT_TRUE(fieldsVar.is<JsonObject>());

    JsonDocument fields;
    fields.set(fieldsVar);

    // Step 5: Check if service message (as in onLinkPacket)
    TEST_ASSERT_TRUE(ServiceMessage::isServiceMessage(fields));

    // Step 6: Parse ServiceMessage (as in handleServiceMessage)
    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::TRUST_OFFER, svcMsg.msg_type);

    // Step 7: Deserialize TrustOfferPayload (as in handleTrustOffer)
    TrustOfferPayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());
    TEST_ASSERT_EQUAL_STRING("HomeCompanionServer", decoded.server_name.c_str());

    // Step 8: Store in TrustedServers (as in handleTrustOffer)
    RNS::Bytes sourceHash;
    sourceHash.assignHex("abcd1234abcd1234abcd1234abcd1234");

    servers.addPendingOffer(sourceHash, decoded.server_name, decoded.services);

    // Step 9: Verify it was stored correctly
    auto pending = servers.getPendingOffers();
    TEST_ASSERT_EQUAL(1, pending.size());
    TEST_ASSERT_EQUAL_STRING("HomeCompanionServer", pending[0].name.c_str());
    TEST_ASSERT_EQUAL(2, pending[0].services.size());
    TEST_ASSERT_EQUAL_STRING("ntp", pending[0].services[0].c_str());
    TEST_ASSERT_EQUAL_STRING("search", pending[0].services[1].c_str());
    TEST_ASSERT_EQUAL(TrustStatus::PENDING, pending[0].status);

    // Step 10: Verify persistence by reloading
    TrustedServers newServers;
    newServers.init(&testFs);

    auto reloaded = newServers.getPendingOffers();
    TEST_ASSERT_EQUAL(1, reloaded.size());
    TEST_ASSERT_EQUAL_STRING("HomeCompanionServer", reloaded[0].name.c_str());

    // Cleanup
    std::string filePath = std::string(testDir) + "/trusted_servers.json";
    remove(filePath.c_str());
    rmdir(testDir);
}

/**
 * Test accepting a trust offer and the full flow.
 */
void test_full_trust_accept_flow(void) {
    char dirTemplate[] = "/tmp/test_accept_XXXXXX";
    char* testDir = mkdtemp(dirTemplate);
    TEST_ASSERT_NOT_NULL(testDir);

    FS testFs(testDir);
    TrustedServers servers;
    servers.init(&testFs);

    // Add a pending offer
    RNS::Bytes serverHash;
    serverHash.assignHex("fedcba9876543210fedcba9876543210");

    std::vector<std::string> svc = {"ntp", "search"};
    servers.addPendingOffer(serverHash, "TestServer", svc);

    // Verify pending
    TEST_ASSERT_TRUE(servers.hasPendingOffer("fedcba9876543210fedcba9876543210"));
    TEST_ASSERT_FALSE(servers.isTrusted(std::string("fedcba9876543210fedcba9876543210")));

    // Accept the offer
    bool accepted = servers.acceptOffer("fedcba9876543210fedcba9876543210");
    TEST_ASSERT_TRUE(accepted);

    // Verify trusted
    TEST_ASSERT_FALSE(servers.hasPendingOffer("fedcba9876543210fedcba9876543210"));
    TEST_ASSERT_TRUE(servers.isTrusted(std::string("fedcba9876543210fedcba9876543210")));

    // Verify services preserved
    const TrustedServer* server = servers.getServer("fedcba9876543210fedcba9876543210");
    TEST_ASSERT_NOT_NULL(server);
    TEST_ASSERT_EQUAL(2, server->services.size());

    // Cleanup
    std::string filePath = std::string(testDir) + "/trusted_servers.json";
    remove(filePath.c_str());
    rmdir(testDir);
}

/**
 * Simulate Python sending binary strings (use_bin_type=True).
 * Python's msgpack with use_bin_type=True encodes strings as bin, not str.
 * ArduinoJson should handle both.
 */
void test_decode_python_binary_strings(void) {
    // Python with use_bin_type=True would encode server_name as binary
    // Let's verify ArduinoJson's MsgPackBinary handling works

    JsonDocument doc;
    // Simulate Python encoding {"server_name": b"TestServer", "services": [b"ntp"]}
    doc["server_name"] = MsgPackBinary((const uint8_t*)"TestServer", 10);
    JsonArray svc = doc["services"].to<JsonArray>();
    svc.add(MsgPackBinary((const uint8_t*)"ntp", 3));

    uint8_t buf[128];
    size_t len = serializeMsgPack(doc, buf, sizeof(buf));

    // Now deserialize and verify our safeGetString helper would work
    JsonDocument decoded;
    deserializeMsgPack(decoded, buf, len);

    // Check that we can read binary as string
    if (decoded["server_name"].is<MsgPackBinary>()) {
        MsgPackBinary bin = decoded["server_name"].as<MsgPackBinary>();
        std::string name((const char*)bin.data(), bin.size());
        TEST_ASSERT_EQUAL_STRING("TestServer", name.c_str());
    } else {
        // ArduinoJson might auto-convert, which is also fine
        TEST_ASSERT_EQUAL_STRING("TestServer", decoded["server_name"].as<const char*>());
    }
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Message type enum tests
    RUN_TEST(test_message_type_trust_offer_value);
    RUN_TEST(test_message_type_trust_accept_value);
    RUN_TEST(test_message_type_trust_revoke_value);
    RUN_TEST(test_message_type_ntp_request_value);
    RUN_TEST(test_message_type_ntp_response_value);
    RUN_TEST(test_message_type_search_request_value);
    RUN_TEST(test_message_type_search_response_value);
    RUN_TEST(test_message_type_map_tile_request_value);
    RUN_TEST(test_message_type_map_tile_response_value);
    RUN_TEST(test_message_type_map_route_request_value);
    RUN_TEST(test_message_type_map_route_response_value);
    RUN_TEST(test_message_type_map_geocode_request_value);
    RUN_TEST(test_message_type_map_geocode_response_value);

    // Propagation message type tests
    RUN_TEST(test_message_type_prop_sync_request_value);
    RUN_TEST(test_message_type_prop_sync_response_value);
    RUN_TEST(test_message_type_prop_msg_deliver_value);
    RUN_TEST(test_message_type_prop_submit_request_value);
    RUN_TEST(test_message_type_prop_submit_response_value);

    // TrustOfferPayload tests
    RUN_TEST(test_trust_offer_serialize_deserialize);
    RUN_TEST(test_trust_offer_empty_services);
    RUN_TEST(test_trust_offer_canonical_structure);

    // TrustAcceptPayload tests
    RUN_TEST(test_trust_accept_serialize_deserialize);
    RUN_TEST(test_trust_accept_canonical_structure);

    // NTPRequestPayload tests
    RUN_TEST(test_ntp_request_serialize_deserialize);
    RUN_TEST(test_ntp_request_canonical_structure);

    // NTPResponsePayload tests
    RUN_TEST(test_ntp_response_serialize_deserialize);
    RUN_TEST(test_ntp_response_canonical_structure);

    // SearchRequestPayload tests
    RUN_TEST(test_search_request_serialize_deserialize);
    RUN_TEST(test_search_request_default_max_results);

    // SearchResponsePayload tests
    RUN_TEST(test_search_response_with_results);
    RUN_TEST(test_search_response_with_error);
    RUN_TEST(test_search_response_empty_results);

    // MapTileRequestPayload tests
    RUN_TEST(test_map_tile_request_serialize_deserialize);
    RUN_TEST(test_map_tile_request_canonical_structure);

    // MapTileResponsePayload tests
    RUN_TEST(test_map_tile_response_serialize_deserialize);
    RUN_TEST(test_map_tile_response_with_error);

    // MapRouteRequestPayload tests
    RUN_TEST(test_map_route_request_serialize_deserialize);
    RUN_TEST(test_map_route_request_canonical_structure);

    // MapRouteResponsePayload tests
    RUN_TEST(test_map_route_response_serialize_deserialize);
    RUN_TEST(test_map_route_response_with_error);

    // MapGeocodeRequestPayload tests
    RUN_TEST(test_map_geocode_request_serialize_deserialize);
    RUN_TEST(test_map_geocode_request_without_bias);

    // MapGeocodeResponsePayload tests
    RUN_TEST(test_map_geocode_response_serialize_deserialize);
    RUN_TEST(test_map_geocode_response_with_error);
    RUN_TEST(test_map_geocode_response_empty_results);

    // PropSyncRequestPayload tests
    RUN_TEST(test_prop_sync_request_serialize_deserialize);
    RUN_TEST(test_prop_sync_request_empty_known_ids);

    // PropSyncResponsePayload tests
    RUN_TEST(test_prop_sync_response_serialize_deserialize);
    RUN_TEST(test_prop_sync_response_with_error);

    // PropMsgDeliverPayload tests
    RUN_TEST(test_prop_msg_deliver_serialize_deserialize);

    // PropSubmitRequestPayload tests
    RUN_TEST(test_prop_submit_request_serialize_deserialize);

    // PropSubmitResponsePayload tests
    RUN_TEST(test_prop_submit_response_accepted);
    RUN_TEST(test_prop_submit_response_rejected);

    // ServiceMessage tests
    RUN_TEST(test_service_message_is_service_message_true);
    RUN_TEST(test_service_message_is_service_message_false);
    RUN_TEST(test_service_message_from_fields);
    RUN_TEST(test_service_message_to_fields);

    // Cross-compatibility tests
    RUN_TEST(test_cross_compat_trust_offer_encoding);
    RUN_TEST(test_cross_compat_ntp_response_encoding);
    RUN_TEST(test_cross_compat_full_message_structure);

    // LXMF message flow simulation tests
    RUN_TEST(test_lxmf_payload_with_service_fields);
    RUN_TEST(test_lxmf_payload_without_service_fields);
    RUN_TEST(test_lxmf_payload_with_empty_fields);
    RUN_TEST(test_lxmf_ntp_response_flow);
    RUN_TEST(test_lxmf_search_response_flow);
    RUN_TEST(test_partial_fields_not_service_message);
    RUN_TEST(test_partial_fields_missing_msg_type);
    RUN_TEST(test_decode_python_binary_strings);

    // Full integration tests
    RUN_TEST(test_full_trust_offer_integration);
    RUN_TEST(test_full_trust_accept_flow);

    return UNITY_END();
}
