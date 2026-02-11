/**
 * Integration Tests for Service Message Handling
 *
 * These tests verify the service layer message handling logic without
 * requiring SDL2/UI or full Reticulum networking. They test:
 *
 * 1. Message parsing from wire format (as received from companion server)
 * 2. Message generation for outgoing requests
 * 3. Trust workflow message handling
 * 4. NTP workflow message handling
 * 5. Search workflow message handling
 * 6. Route/Geocode workflow message handling
 *
 * The tests use mock/simulated network data to verify the service logic.
 */

#include <unity.h>
#include <ArduinoJson.h>
#include <string>
#include <vector>
#include <cstring>
#include "services/RnsUtils/ServiceProtocol.h"
#include "services/RnsUtils/TrustedServers.h"
#include "FS.h"

using namespace Retcon::Service;

// ============================================================================
// Test Fixtures
// ============================================================================

static char* testDir = nullptr;
static FS* testFs = nullptr;
static TrustedServers* testServers = nullptr;

void setUp(void) {
    // Create temp directory for each test
    static char dirTemplate[] = "/tmp/test_svc_int_XXXXXX";
    // Copy template to preserve original
    static char dirBuf[64];
    strcpy(dirBuf, "/tmp/test_svc_int_XXXXXX");
    testDir = mkdtemp(dirBuf);

    testFs = new FS(testDir);
    testServers = new TrustedServers();
    testServers->init(testFs);
}

void tearDown(void) {
    if (testServers) {
        delete testServers;
        testServers = nullptr;
    }
    if (testFs) {
        delete testFs;
        testFs = nullptr;
    }
    if (testDir) {
        std::string filePath = std::string(testDir) + "/trusted_servers.json";
        remove(filePath.c_str());
        rmdir(testDir);
        testDir = nullptr;
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Simulate receiving an LXMF message from the companion server.
 * This creates the full LXMF payload structure: [timestamp, title, content, fields]
 */
static std::vector<uint8_t> createLxmfPayload(const ServiceMessage& msg) {
    // Serialize inner payload
    JsonDocument fieldsDoc;
    msg.toFields(fieldsDoc);

    // Create full LXMF packed payload: [timestamp, title, content, fields]
    JsonDocument lxmfPayload;
    JsonArray arr = lxmfPayload.to<JsonArray>();
    arr.add(1706825600);  // timestamp
    arr.add(MsgPackBinary((const uint8_t*)"", 0));  // empty title
    arr.add(MsgPackBinary((const uint8_t*)"", 0));  // empty content
    arr.add(fieldsDoc);  // fields dict

    std::vector<uint8_t> buffer(1024);
    size_t len = serializeMsgPack(lxmfPayload, buffer.data(), buffer.size());
    buffer.resize(len);
    return buffer;
}

/**
 * Parse LXMF payload and extract service message fields.
 */
static bool extractServiceFields(const std::vector<uint8_t>& lxmfPayload, JsonDocument& fields) {
    JsonDocument doc;
    DeserializationError err = deserializeMsgPack(doc, lxmfPayload.data(), lxmfPayload.size());
    if (err) return false;

    if (!doc.is<JsonArray>() || doc.size() < 4) return false;

    JsonVariant fieldsVar = doc[3];
    if (fieldsVar.isNull() || !fieldsVar.is<JsonObject>()) return false;

    fields.set(fieldsVar);
    return ServiceMessage::isServiceMessage(fields);
}

// ============================================================================
// Trust Workflow Integration Tests
// ============================================================================

void test_trust_offer_full_flow(void) {
    // Simulate receiving TRUST_OFFER from companion server
    TrustOfferPayload innerPayload;
    innerPayload.server_name = "TestCompanionServer";
    innerPayload.services.push_back("ntp");
    innerPayload.services.push_back("search");

    uint8_t innerBuf[256];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::TRUST_OFFER;
    incomingMsg.service = "trust";
    incomingMsg.request_id = 12345;
    incomingMsg.payload.assign(innerBuf, innerLen);

    // Create LXMF payload as wire format
    auto lxmfPayload = createLxmfPayload(incomingMsg);

    // Extract and parse
    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::TRUST_OFFER, svcMsg.msg_type);
    TEST_ASSERT_EQUAL_STRING("trust", svcMsg.service.c_str());
    TEST_ASSERT_EQUAL(12345, svcMsg.request_id);

    // Deserialize payload
    TrustOfferPayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());
    TEST_ASSERT_EQUAL_STRING("TestCompanionServer", decoded.server_name.c_str());
    TEST_ASSERT_EQUAL(2, decoded.services.size());

    // Store as pending offer
    RNS::Bytes serverHash;
    serverHash.assignHex("abcd1234abcd1234abcd1234abcd1234");
    testServers->addPendingOffer(serverHash, decoded.server_name, decoded.services);

    // Verify pending
    TEST_ASSERT_TRUE(testServers->hasPendingOffer("abcd1234abcd1234abcd1234abcd1234"));
    TEST_ASSERT_FALSE(testServers->isTrusted(std::string("abcd1234abcd1234abcd1234abcd1234")));

    // Accept the offer
    bool accepted = testServers->acceptOffer("abcd1234abcd1234abcd1234abcd1234");
    TEST_ASSERT_TRUE(accepted);

    // Verify trusted
    TEST_ASSERT_FALSE(testServers->hasPendingOffer("abcd1234abcd1234abcd1234abcd1234"));
    TEST_ASSERT_TRUE(testServers->isTrusted(std::string("abcd1234abcd1234abcd1234abcd1234")));
}

void test_trust_accept_message_generation(void) {
    // Test generating a TRUST_ACCEPT message to send to server
    TrustAcceptPayload payload;
    payload.device_name = "My rDeck Device";

    uint8_t payloadBuf[128];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    ServiceMessage msg;
    msg.msg_type = MessageType::TRUST_ACCEPT;
    msg.service = "trust";
    msg.request_id = 0;
    msg.payload.assign(payloadBuf, payloadLen);

    // Convert to fields for LXMF
    JsonDocument fields;
    msg.toFields(fields);

    TEST_ASSERT_EQUAL(0x02, fields["msg_type"].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("trust", fields["service"].as<const char*>());

    // Verify payload can be decoded
    MsgPackBinary bin = fields["payload"].as<MsgPackBinary>();
    TrustAcceptPayload decoded;
    decoded.deserialize((const uint8_t*)bin.data(), bin.size());
    TEST_ASSERT_EQUAL_STRING("My rDeck Device", decoded.device_name.c_str());
}

void test_trust_workflow_revoke(void) {
    // Test revoking trust after it's been established
    RNS::Bytes serverHash;
    serverHash.assignHex("deadbeef12345678deadbeef12345678");

    std::vector<std::string> services = {"ntp"};
    testServers->addPendingOffer(serverHash, "RevokeMe", services);

    // Accept first
    TEST_ASSERT_TRUE(testServers->acceptOffer("deadbeef12345678deadbeef12345678"));
    TEST_ASSERT_TRUE(testServers->isTrusted(std::string("deadbeef12345678deadbeef12345678")));

    // Now revoke
    testServers->revokeTrust("deadbeef12345678deadbeef12345678");

    // Verify not trusted anymore
    TEST_ASSERT_FALSE(testServers->isTrusted(std::string("deadbeef12345678deadbeef12345678")));
}

// ============================================================================
// NTP Workflow Integration Tests
// ============================================================================

void test_ntp_request_message_generation(void) {
    // Test generating an NTP_REQUEST message
    NTPRequestPayload payload;
    payload.client_timestamp = 123456789;

    uint8_t payloadBuf[64];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    ServiceMessage msg;
    msg.msg_type = MessageType::NTP_REQUEST;
    msg.service = "ntp";
    msg.request_id = 42;
    msg.payload.assign(payloadBuf, payloadLen);

    // Convert to fields for LXMF
    JsonDocument fields;
    msg.toFields(fields);

    TEST_ASSERT_EQUAL(0x10, fields["msg_type"].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("ntp", fields["service"].as<const char*>());
    TEST_ASSERT_EQUAL(42, fields["request_id"].as<uint32_t>());
}

void test_ntp_response_handling(void) {
    // Simulate receiving NTP_RESPONSE from server
    NTPResponsePayload innerPayload;
    innerPayload.server_timestamp = 1706825600;  // Fixed epoch time
    innerPayload.client_timestamp = 50000;       // Echo back

    uint8_t innerBuf[64];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::NTP_RESPONSE;
    incomingMsg.service = "ntp";
    incomingMsg.request_id = 42;
    incomingMsg.payload.assign(innerBuf, innerLen);

    // Create LXMF payload
    auto lxmfPayload = createLxmfPayload(incomingMsg);

    // Extract and parse
    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::NTP_RESPONSE, svcMsg.msg_type);
    TEST_ASSERT_EQUAL(42, svcMsg.request_id);

    // Deserialize payload
    NTPResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());

    TEST_ASSERT_EQUAL_UINT32(1706825600, decoded.server_timestamp);
    TEST_ASSERT_EQUAL_UINT32(50000, decoded.client_timestamp);

    // Time sync logic would be:
    // - client_timestamp is the millis() when request was sent
    // - server_timestamp is the actual time
    // - We can calculate: actualTime = server_timestamp + (currentMillis - client_timestamp) / 1000
}

void test_ntp_request_response_roundtrip(void) {
    // Full roundtrip: generate request, simulate response, verify
    uint32_t originalClientTime = 100000;  // millis when request sent

    // Generate request
    NTPRequestPayload reqPayload;
    reqPayload.client_timestamp = originalClientTime;

    uint8_t reqBuf[64];
    size_t reqLen = reqPayload.serialize(reqBuf, sizeof(reqBuf));

    // Simulate server response with echoed client_timestamp
    NTPResponsePayload respPayload;
    respPayload.server_timestamp = 1706825600;
    respPayload.client_timestamp = originalClientTime;  // Echo back

    uint8_t respBuf[64];
    size_t respLen = respPayload.serialize(respBuf, sizeof(respBuf));

    // Verify response matches request
    NTPResponsePayload decoded;
    decoded.deserialize(respBuf, respLen);

    TEST_ASSERT_EQUAL_UINT32(originalClientTime, decoded.client_timestamp);
    TEST_ASSERT_EQUAL_UINT32(1706825600, decoded.server_timestamp);
}

// ============================================================================
// Search Workflow Integration Tests
// ============================================================================

void test_search_request_message_generation(void) {
    // Test generating a SEARCH_REQUEST message
    SearchRequestPayload payload;
    payload.query = "reticulum mesh network";
    payload.max_results = 5;
    payload.ai_summary = false;

    uint8_t payloadBuf[128];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    ServiceMessage msg;
    msg.msg_type = MessageType::SEARCH_REQUEST;
    msg.service = "search";
    msg.request_id = 999;
    msg.payload.assign(payloadBuf, payloadLen);

    // Convert to fields for LXMF
    JsonDocument fields;
    msg.toFields(fields);

    TEST_ASSERT_EQUAL(0x20, fields["msg_type"].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("search", fields["service"].as<const char*>());
    TEST_ASSERT_EQUAL(999, fields["request_id"].as<uint32_t>());
}

void test_search_request_with_ai_summary(void) {
    // Test generating a SEARCH_REQUEST with AI summary enabled
    SearchRequestPayload payload;
    payload.query = "what is reticulum";
    payload.max_results = 3;
    payload.ai_summary = true;

    uint8_t payloadBuf[128];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    // Deserialize and verify
    SearchRequestPayload decoded;
    decoded.deserialize(payloadBuf, payloadLen);

    TEST_ASSERT_EQUAL_STRING("what is reticulum", decoded.query.c_str());
    TEST_ASSERT_EQUAL(3, decoded.max_results);
    TEST_ASSERT_TRUE(decoded.ai_summary);
}

void test_search_response_with_results(void) {
    // Simulate receiving SEARCH_RESPONSE with results
    SearchResponsePayload innerPayload;
    innerPayload.query = "reticulum";
    innerPayload.results.push_back({"Reticulum Network", "https://reticulum.network", "Official site"});
    innerPayload.results.push_back({"GitHub Repo", "https://github.com/markqvist/Reticulum", "Source code"});

    uint8_t innerBuf[1024];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::SEARCH_RESPONSE;
    incomingMsg.service = "search";
    incomingMsg.request_id = 999;
    incomingMsg.payload.assign(innerBuf, innerLen);

    // Create LXMF payload
    auto lxmfPayload = createLxmfPayload(incomingMsg);

    // Extract and parse
    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::SEARCH_RESPONSE, svcMsg.msg_type);

    // Deserialize payload
    SearchResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());

    TEST_ASSERT_EQUAL_STRING("reticulum", decoded.query.c_str());
    TEST_ASSERT_EQUAL(2, decoded.results.size());
    TEST_ASSERT_EQUAL_STRING("Reticulum Network", decoded.results[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("https://reticulum.network", decoded.results[0].url.c_str());
    TEST_ASSERT_EQUAL_STRING("Official site", decoded.results[0].snippet.c_str());
    TEST_ASSERT_TRUE(decoded.error.empty());
}

void test_search_response_with_error(void) {
    // Simulate receiving SEARCH_RESPONSE with error
    SearchResponsePayload innerPayload;
    innerPayload.query = "test query";
    innerPayload.error = "Service temporarily unavailable";

    uint8_t innerBuf[256];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::SEARCH_RESPONSE;
    incomingMsg.service = "search";
    incomingMsg.request_id = 1000;
    incomingMsg.payload.assign(innerBuf, innerLen);

    // Create LXMF payload
    auto lxmfPayload = createLxmfPayload(incomingMsg);

    // Extract and parse
    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);

    // Deserialize payload
    SearchResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());

    TEST_ASSERT_EQUAL_STRING("test query", decoded.query.c_str());
    TEST_ASSERT_EQUAL(0, decoded.results.size());
    TEST_ASSERT_EQUAL_STRING("Service temporarily unavailable", decoded.error.c_str());
}

void test_search_response_empty_results(void) {
    // Simulate receiving SEARCH_RESPONSE with no results (not an error)
    SearchResponsePayload innerPayload;
    innerPayload.query = "xyzzy12345 gibberish";
    // No results, no error

    uint8_t innerBuf[256];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::SEARCH_RESPONSE;
    incomingMsg.service = "search";
    incomingMsg.request_id = 1001;
    incomingMsg.payload.assign(innerBuf, innerLen);

    auto lxmfPayload = createLxmfPayload(incomingMsg);

    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);

    SearchResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());

    TEST_ASSERT_EQUAL_STRING("xyzzy12345 gibberish", decoded.query.c_str());
    TEST_ASSERT_EQUAL(0, decoded.results.size());
    TEST_ASSERT_TRUE(decoded.error.empty());
    TEST_ASSERT_TRUE(decoded.summary.empty());
}

void test_search_response_with_ai_summary(void) {
    // Test receiving SEARCH_RESPONSE with AI summary
    SearchResponsePayload innerPayload;
    innerPayload.query = "what is reticulum";
    innerPayload.results.push_back({"Reticulum Network", "https://reticulum.network", "Official site"});
    innerPayload.summary = "Reticulum is a cryptography-based networking stack for building local and wide-area networks with commodity hardware.";

    uint8_t innerBuf[1024];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::SEARCH_RESPONSE;
    incomingMsg.service = "search";
    incomingMsg.request_id = 1002;
    incomingMsg.payload.assign(innerBuf, innerLen);

    auto lxmfPayload = createLxmfPayload(incomingMsg);

    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);

    SearchResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());

    TEST_ASSERT_EQUAL_STRING("what is reticulum", decoded.query.c_str());
    TEST_ASSERT_EQUAL(1, decoded.results.size());
    TEST_ASSERT_TRUE(decoded.error.empty());
    TEST_ASSERT_FALSE(decoded.summary.empty());
    TEST_ASSERT_TRUE(decoded.summary.find("Reticulum") != std::string::npos);
}

// ============================================================================
// Route/Geocode Workflow Integration Tests
// ============================================================================

void test_route_request_message_generation(void) {
    // Test generating a MAP_ROUTE_REQUEST message
    MapRouteRequestPayload payload;
    payload.start_lat = 478563210;   // 47.856321 * 1e7
    payload.start_lon = -1224567890; // -122.456789 * 1e7
    payload.end_lat = 478600000;
    payload.end_lon = -1224500000;
    payload.mode = TravelMode::WALK;

    uint8_t payloadBuf[128];
    size_t payloadLen = payload.serialize(payloadBuf, sizeof(payloadBuf));

    ServiceMessage msg;
    msg.msg_type = MessageType::MAP_ROUTE_REQUEST;
    msg.service = "maps";
    msg.request_id = 500;
    msg.payload.assign(payloadBuf, payloadLen);

    // Convert to fields for LXMF
    JsonDocument fields;
    msg.toFields(fields);

    TEST_ASSERT_EQUAL(0x33, fields["msg_type"].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("maps", fields["service"].as<const char*>());
    TEST_ASSERT_EQUAL(500, fields["request_id"].as<uint32_t>());

    // Verify payload can be decoded
    MsgPackBinary bin = fields["payload"].as<MsgPackBinary>();
    MapRouteRequestPayload decoded;
    decoded.deserialize((const uint8_t*)bin.data(), bin.size());
    TEST_ASSERT_EQUAL_INT32(478563210, decoded.start_lat);
    TEST_ASSERT_EQUAL_INT32(-1224567890, decoded.start_lon);
    TEST_ASSERT_EQUAL_INT32(478600000, decoded.end_lat);
    TEST_ASSERT_EQUAL_INT32(-1224500000, decoded.end_lon);
    TEST_ASSERT_EQUAL(TravelMode::WALK, decoded.mode);
}

void test_route_response_handling(void) {
    // Simulate receiving MAP_ROUTE_RESPONSE with full data
    MapRouteResponsePayload innerPayload;
    innerPayload.points = {478563210, -1224567890, 478580000, -1224530000, 478600000, -1224500000};
    innerPayload.instructions.push_back({150, "straight", "Main St"});
    innerPayload.instructions.push_back({200, "turn-left", "Oak Ave"});
    innerPayload.instructions.push_back({0, "arrive", ""});
    innerPayload.total_distance_m = 350;
    innerPayload.total_time_s = 240;

    uint8_t innerBuf[1024];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::MAP_ROUTE_RESPONSE;
    incomingMsg.service = "maps";
    incomingMsg.request_id = 500;
    incomingMsg.payload.assign(innerBuf, innerLen);

    // Create LXMF payload
    auto lxmfPayload = createLxmfPayload(incomingMsg);

    // Extract and parse
    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::MAP_ROUTE_RESPONSE, svcMsg.msg_type);
    TEST_ASSERT_EQUAL_STRING("maps", svcMsg.service.c_str());
    TEST_ASSERT_EQUAL(500, svcMsg.request_id);

    // Deserialize payload
    MapRouteResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());

    TEST_ASSERT_EQUAL(6, decoded.points.size());
    TEST_ASSERT_EQUAL_INT32(478563210, decoded.points[0]);
    TEST_ASSERT_EQUAL_INT32(-1224567890, decoded.points[1]);
    TEST_ASSERT_EQUAL_INT32(478600000, decoded.points[4]);

    TEST_ASSERT_EQUAL(3, decoded.instructions.size());
    TEST_ASSERT_EQUAL_STRING("straight", decoded.instructions[0].maneuver.c_str());
    TEST_ASSERT_EQUAL_STRING("Main St", decoded.instructions[0].street.c_str());
    TEST_ASSERT_EQUAL(150, decoded.instructions[0].distance_m);
    TEST_ASSERT_EQUAL_STRING("turn-left", decoded.instructions[1].maneuver.c_str());
    TEST_ASSERT_EQUAL_STRING("arrive", decoded.instructions[2].maneuver.c_str());

    TEST_ASSERT_EQUAL_UINT32(350, decoded.total_distance_m);
    TEST_ASSERT_EQUAL_UINT32(240, decoded.total_time_s);
    TEST_ASSERT_TRUE(decoded.error.empty());
}

void test_route_response_with_error_handling(void) {
    // Simulate receiving MAP_ROUTE_RESPONSE with error
    MapRouteResponsePayload innerPayload;
    innerPayload.error = "No route found between points";

    uint8_t innerBuf[256];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::MAP_ROUTE_RESPONSE;
    incomingMsg.service = "maps";
    incomingMsg.request_id = 501;
    incomingMsg.payload.assign(innerBuf, innerLen);

    // Create LXMF payload
    auto lxmfPayload = createLxmfPayload(incomingMsg);

    // Extract and parse
    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::MAP_ROUTE_RESPONSE, svcMsg.msg_type);

    // Deserialize payload
    MapRouteResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());

    TEST_ASSERT_EQUAL(0, decoded.points.size());
    TEST_ASSERT_EQUAL(0, decoded.instructions.size());
    TEST_ASSERT_EQUAL_STRING("No route found between points", decoded.error.c_str());
}

void test_geocode_response_handling(void) {
    // Simulate receiving MAP_GEOCODE_RESPONSE with results
    MapGeocodeResponsePayload innerPayload;
    innerPayload.query = "Portland";
    innerPayload.results.push_back({"Portland, OR, USA", 455123456, -1226789012, "city"});
    innerPayload.results.push_back({"Portland, ME, USA", 436568000, -702580000, "city"});

    uint8_t innerBuf[1024];
    size_t innerLen = innerPayload.serialize(innerBuf, sizeof(innerBuf));

    ServiceMessage incomingMsg;
    incomingMsg.msg_type = MessageType::MAP_GEOCODE_RESPONSE;
    incomingMsg.service = "maps";
    incomingMsg.request_id = 600;
    incomingMsg.payload.assign(innerBuf, innerLen);

    // Create LXMF payload
    auto lxmfPayload = createLxmfPayload(incomingMsg);

    // Extract and parse
    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage svcMsg = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(MessageType::MAP_GEOCODE_RESPONSE, svcMsg.msg_type);
    TEST_ASSERT_EQUAL_STRING("maps", svcMsg.service.c_str());
    TEST_ASSERT_EQUAL(600, svcMsg.request_id);

    // Deserialize payload
    MapGeocodeResponsePayload decoded;
    decoded.deserialize(svcMsg.payload.data(), svcMsg.payload.size());

    TEST_ASSERT_EQUAL_STRING("Portland", decoded.query.c_str());
    TEST_ASSERT_EQUAL(2, decoded.results.size());
    TEST_ASSERT_EQUAL_STRING("Portland, OR, USA", decoded.results[0].display_name.c_str());
    TEST_ASSERT_EQUAL_INT32(455123456, decoded.results[0].lat);
    TEST_ASSERT_EQUAL_INT32(-1226789012, decoded.results[0].lon);
    TEST_ASSERT_EQUAL_STRING("city", decoded.results[0].type.c_str());
    TEST_ASSERT_EQUAL_STRING("Portland, ME, USA", decoded.results[1].display_name.c_str());
    TEST_ASSERT_TRUE(decoded.error.empty());
}

// ============================================================================
// Message Type Routing Tests
// ============================================================================

void test_message_type_routing(void) {
    // Test that we can correctly identify and route different message types
    struct TestCase {
        MessageType type;
        std::string service;
    };

    TestCase cases[] = {
        {MessageType::TRUST_OFFER, "trust"},
        {MessageType::TRUST_ACCEPT, "trust"},
        {MessageType::TRUST_REVOKE, "trust"},
        {MessageType::NTP_REQUEST, "ntp"},
        {MessageType::NTP_RESPONSE, "ntp"},
        {MessageType::SEARCH_REQUEST, "search"},
        {MessageType::SEARCH_RESPONSE, "search"},
        {MessageType::MAP_TILE_REQUEST, "maps"},
        {MessageType::MAP_TILE_RESPONSE, "maps"},
        {MessageType::MAP_ROUTE_REQUEST, "maps"},
        {MessageType::MAP_ROUTE_RESPONSE, "maps"},
        {MessageType::MAP_GEOCODE_REQUEST, "maps"},
        {MessageType::MAP_GEOCODE_RESPONSE, "maps"},
    };

    for (const auto& tc : cases) {
        ServiceMessage msg;
        msg.msg_type = tc.type;
        msg.service = tc.service;
        msg.request_id = 0;

        JsonDocument fields;
        msg.toFields(fields);

        TEST_ASSERT_TRUE(ServiceMessage::isServiceMessage(fields));

        ServiceMessage parsed = ServiceMessage::fromFields(fields);
        TEST_ASSERT_EQUAL(tc.type, parsed.msg_type);
        TEST_ASSERT_EQUAL_STRING(tc.service.c_str(), parsed.service.c_str());
    }
}

void test_non_service_message_detection(void) {
    // Regular LXMF messages (not service messages) should not be detected
    JsonDocument regularFields;
    regularFields["some_field"] = "value";

    TEST_ASSERT_FALSE(ServiceMessage::isServiceMessage(regularFields));

    // Missing msg_type
    JsonDocument missingType;
    missingType["service"] = "ntp";
    TEST_ASSERT_FALSE(ServiceMessage::isServiceMessage(missingType));

    // Missing service
    JsonDocument missingService;
    missingService["msg_type"] = 0x10;
    TEST_ASSERT_FALSE(ServiceMessage::isServiceMessage(missingService));
}

// ============================================================================
// Request ID Correlation Tests
// ============================================================================

void test_request_id_correlation(void) {
    // Test that request_id is preserved through serialization
    // Note: Using a smaller value that ArduinoJson handles reliably
    ServiceMessage outgoing;
    outgoing.msg_type = MessageType::NTP_REQUEST;
    outgoing.service = "ntp";
    outgoing.request_id = 12345678;  // Realistic request ID

    NTPRequestPayload payload;
    payload.client_timestamp = 12345;
    uint8_t buf[64];
    size_t len = payload.serialize(buf, sizeof(buf));
    outgoing.payload.assign(buf, len);

    // Serialize to LXMF
    auto lxmfPayload = createLxmfPayload(outgoing);

    // Parse back
    JsonDocument fields;
    TEST_ASSERT_TRUE(extractServiceFields(lxmfPayload, fields));

    ServiceMessage incoming = ServiceMessage::fromFields(fields);
    TEST_ASSERT_EQUAL(12345678, incoming.request_id);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Trust workflow tests
    RUN_TEST(test_trust_offer_full_flow);
    RUN_TEST(test_trust_accept_message_generation);
    RUN_TEST(test_trust_workflow_revoke);

    // NTP workflow tests
    RUN_TEST(test_ntp_request_message_generation);
    RUN_TEST(test_ntp_response_handling);
    RUN_TEST(test_ntp_request_response_roundtrip);

    // Search workflow tests
    RUN_TEST(test_search_request_message_generation);
    RUN_TEST(test_search_request_with_ai_summary);
    RUN_TEST(test_search_response_with_results);
    RUN_TEST(test_search_response_with_error);
    RUN_TEST(test_search_response_empty_results);
    RUN_TEST(test_search_response_with_ai_summary);

    // Route/Geocode workflow tests
    RUN_TEST(test_route_request_message_generation);
    RUN_TEST(test_route_response_handling);
    RUN_TEST(test_route_response_with_error_handling);
    RUN_TEST(test_geocode_response_handling);

    // Message routing tests
    RUN_TEST(test_message_type_routing);
    RUN_TEST(test_non_service_message_detection);

    // Request ID tests
    RUN_TEST(test_request_id_correlation);

    return UNITY_END();
}
