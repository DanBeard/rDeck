#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string>
#include <vector>
#include "Bytes.h"

/**
 * Service Protocol for companion server communication
 *
 * Messages use LXMF's fields with msgpack encoding:
 * {
 *     "msg_type": <uint8>,
 *     "service": <string>,
 *     "payload": <msgpack bytes>,
 *     "request_id": <uint32>
 * }
 */

namespace Retcon::Service {

// Message type enum matching Python companion server
enum class MessageType : uint8_t {
    // Trust management
    TRUST_OFFER = 0x01,
    TRUST_ACCEPT = 0x02,
    TRUST_REVOKE = 0x03,

    // NTP service
    NTP_REQUEST = 0x10,
    NTP_RESPONSE = 0x11,

    // Search service
    SEARCH_REQUEST = 0x20,
    SEARCH_RESPONSE = 0x21,
};

// Forward declarations
struct TrustOfferPayload;
struct TrustAcceptPayload;
struct NTPRequestPayload;
struct NTPResponsePayload;
struct SearchRequestPayload;
struct SearchResponsePayload;
struct SearchResult;

/**
 * Trust offer payload - Server -> Device: "I want to serve you"
 */
struct TrustOfferPayload {
    std::string server_name;
    std::vector<std::string> services;

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * Trust accept payload - Device -> Server: "I accept your services"
 */
struct TrustAcceptPayload {
    std::string device_name;

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * NTP request payload - Device -> Server
 */
struct NTPRequestPayload {
    uint32_t client_timestamp;  // Epoch ms for RTT calculation

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * NTP response payload - Server -> Device
 */
struct NTPResponsePayload {
    uint32_t server_timestamp;  // Epoch seconds
    uint32_t client_timestamp;  // Echo back for RTT

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * Search request payload - Device -> Server
 */
struct SearchRequestPayload {
    std::string query;
    uint8_t max_results = 5;

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * A single search result
 */
struct SearchResult {
    std::string title;
    std::string url;
    std::string snippet;
};

/**
 * Search response payload - Server -> Device
 */
struct SearchResponsePayload {
    std::string query;
    std::vector<SearchResult> results;
    std::string error;

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * Base service message structure
 */
struct ServiceMessage {
    MessageType msg_type;
    std::string service;
    RNS::Bytes payload;  // Raw msgpack payload bytes
    uint32_t request_id = 0;

    // Check if LXMF fields contain a service message
    static bool isServiceMessage(JsonDocument& fields);

    // Decode from LXMF fields
    static ServiceMessage fromFields(JsonDocument& fields);

    // Encode to LXMF fields dict (for inclusion in LXMF message)
    void toFields(JsonDocument& fields) const;
};

} // namespace Retcon::Service
