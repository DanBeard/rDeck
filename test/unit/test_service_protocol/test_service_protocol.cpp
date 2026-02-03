/**
 * Unit tests for ServiceProtocol message serialization/deserialization.
 *
 * These tests verify that the C++ implementation can correctly serialize
 * and deserialize service messages, and that the encoding matches the
 * Python companion server for cross-platform compatibility.
 */

#include <unity.h>
#include <ArduinoJson.h>
#include "services/RnsUtils/ServiceProtocol.h"

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

    // ServiceMessage tests
    RUN_TEST(test_service_message_is_service_message_true);
    RUN_TEST(test_service_message_is_service_message_false);
    RUN_TEST(test_service_message_from_fields);
    RUN_TEST(test_service_message_to_fields);

    // Cross-compatibility tests
    RUN_TEST(test_cross_compat_trust_offer_encoding);
    RUN_TEST(test_cross_compat_ntp_response_encoding);
    RUN_TEST(test_cross_compat_full_message_structure);

    return UNITY_END();
}
