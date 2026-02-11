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

    // Maps service
    MAP_TILE_REQUEST = 0x30,
    MAP_TILE_RESPONSE = 0x31,
    MAP_ROUTE_REQUEST = 0x33,
    MAP_ROUTE_RESPONSE = 0x34,
    MAP_GEOCODE_REQUEST = 0x35,
    MAP_GEOCODE_RESPONSE = 0x36,
};

// Forward declarations
struct TrustOfferPayload;
struct TrustAcceptPayload;
struct NTPRequestPayload;
struct NTPResponsePayload;
struct SearchRequestPayload;
struct SearchResponsePayload;
struct SearchResult;
struct MapTileRequestPayload;
struct MapTileResponsePayload;
struct MapRouteRequestPayload;
struct MapRouteResponsePayload;
struct MapRouteInstruction;
struct MapGeocodeRequestPayload;
struct MapGeocodeResponsePayload;
struct MapGeocodeResult;

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
    bool ai_summary = false;  // Request AI-generated summary instead of raw results

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
    std::string summary;  // AI-generated summary (if requested and available)

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

// ============================================================================
// Maps Service Payloads
// ============================================================================

/**
 * Tile format enum - determines tile encoding
 */
enum class TileFormat : uint8_t {
    MONO_RLE = 0,   // 1-bit dithered, RLE compressed (default)
    RAW_1BIT = 1,   // 1-bit dithered, uncompressed (128x128 = 2KB)
};

/**
 * Map tile request payload - Device -> Server
 */
struct MapTileRequestPayload {
    uint8_t z;          // Zoom level (typically 10-16)
    uint32_t x;         // Tile X coordinate
    uint32_t y;         // Tile Y coordinate
    TileFormat format = TileFormat::MONO_RLE;

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * Map tile response payload - Server -> Device
 * Large payloads are transferred via Reticulum Resources automatically.
 */
struct MapTileResponsePayload {
    uint8_t z;
    uint32_t x;
    uint32_t y;
    TileFormat format;
    std::vector<uint8_t> data;  // RLE-compressed 1-bit tile data
    std::string error;          // Error message if failed

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * Travel mode for routing
 */
enum class TravelMode : uint8_t {
    WALK = 0,
    BIKE = 1,
    CAR = 2,
};

/**
 * Map route request payload - Device -> Server
 * Coordinates stored as int32 * 1e7 for precision without floats
 */
struct MapRouteRequestPayload {
    int32_t start_lat;  // Latitude * 1e7
    int32_t start_lon;  // Longitude * 1e7
    int32_t end_lat;
    int32_t end_lon;
    TravelMode mode = TravelMode::WALK;

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * Turn-by-turn instruction
 */
struct MapRouteInstruction {
    uint32_t distance_m;    // Distance in meters to this maneuver
    std::string maneuver;   // "turn-left", "turn-right", "straight", "arrive", etc.
    std::string street;     // Street name (may be empty)
};

/**
 * Map route response payload - Server -> Device
 */
struct MapRouteResponsePayload {
    std::vector<int32_t> points;  // Lat/lon pairs * 1e7 (alternating: lat0, lon0, lat1, lon1, ...)
    std::vector<MapRouteInstruction> instructions;
    uint32_t total_distance_m;    // Total route distance in meters
    uint32_t total_time_s;        // Estimated time in seconds
    std::string error;

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * Map geocode (address search) request payload - Device -> Server
 */
struct MapGeocodeRequestPayload {
    std::string query;          // Search query (address, place name, etc.)
    int32_t bias_lat = 0;       // Optional: bias results near this lat * 1e7
    int32_t bias_lon = 0;       // Optional: bias results near this lon * 1e7
    bool has_bias = false;      // Whether bias coordinates are set
    uint8_t max_results = 5;

    void deserialize(const uint8_t* data, size_t len);
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
};

/**
 * A single geocode result
 */
struct MapGeocodeResult {
    std::string display_name;   // Full formatted address/name
    int32_t lat;                // Latitude * 1e7
    int32_t lon;                // Longitude * 1e7
    std::string type;           // Place type: "city", "street", "house", "poi", etc.
};

/**
 * Map geocode response payload - Server -> Device
 */
struct MapGeocodeResponsePayload {
    std::string query;
    std::vector<MapGeocodeResult> results;
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
    uint8_t retry_count = 0;  // Internal: path-request retry counter (not serialized)

    // Check if LXMF fields contain a service message
    static bool isServiceMessage(JsonDocument& fields);

    // Decode from LXMF fields
    static ServiceMessage fromFields(JsonDocument& fields);

    // Encode to LXMF fields dict (for inclusion in LXMF message)
    void toFields(JsonDocument& fields) const;
};

} // namespace Retcon::Service
