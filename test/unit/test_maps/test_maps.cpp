/**
 * Unit tests for Maps app pure-logic functionality.
 *
 * Tests cover:
 * 1. Coordinate conversion (latLonToTile, tileToLatLon, latLonToPixel)
 * 2. RLE decompression
 * 3. GPS event encoding/decoding round-trip
 * 4. Heading rotation math
 * 5. TileKey comparison operators
 * 6. Zoom bounds clamping
 *
 * These tests do NOT instantiate Maps or depend on LVGL. Instead, they
 * reimplement the pure math/logic functions identically to Maps.cpp to
 * verify correctness of the algorithms against known reference values.
 */

#include <unity.h>
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <string>
#include <cstdio>

// ---------------------------------------------------------------------------
// Standalone reimplementations of Maps pure-logic functions
// (identical to Maps.cpp, but free of LVGL / BaseApp dependencies)
// ---------------------------------------------------------------------------

static const int TILE_SIZE = 128;
static const uint8_t MIN_ZOOM = 10;
static const uint8_t MAX_ZOOM = 18;

struct TileKey {
    uint8_t z;
    uint32_t x;
    uint32_t y;

    bool operator<(const TileKey& other) const {
        if (z != other.z) return z < other.z;
        if (x != other.x) return x < other.x;
        return y < other.y;
    }

    bool operator==(const TileKey& other) const {
        return z == other.z && x == other.x && y == other.y;
    }

    bool operator!=(const TileKey& other) const {
        return !(*this == other);
    }
};

// OSM slippy map tile conversion — identical to Maps::latLonToTile
static void latLonToTile(double lat, double lon, uint8_t z, uint32_t& tileX, uint32_t& tileY) {
    double n = pow(2.0, z);
    tileX = (uint32_t)((lon + 180.0) / 360.0 * n);
    double latRad = lat * M_PI / 180.0;
    tileY = (uint32_t)((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n);
}

// Inverse — identical to Maps::tileToLatLon
static void tileToLatLon(uint32_t tileX, uint32_t tileY, uint8_t z, double& lat, double& lon) {
    double n = pow(2.0, z);
    lon = tileX / n * 360.0 - 180.0;
    double latRad = atan(sinh(M_PI * (1 - 2 * tileY / n)));
    lat = latRad * 180.0 / M_PI;
}

// Pixel-level — identical to Maps::latLonToPixel
static void latLonToPixel(double lat, double lon, uint8_t z, int& pixelX, int& pixelY) {
    double n = pow(2.0, z);
    pixelX = (int)((lon + 180.0) / 360.0 * n * TILE_SIZE);
    double latRad = lat * M_PI / 180.0;
    pixelY = (int)((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n * TILE_SIZE);
}

// RLE decompression — identical to Maps::decompressRle
static std::vector<uint8_t> decompressRle(const std::vector<uint8_t>& rleData) {
    std::vector<uint8_t> result;
    result.reserve(TILE_SIZE * TILE_SIZE / 8);

    for (size_t i = 0; i + 1 < rleData.size(); i += 2) {
        uint8_t count = rleData[i];
        uint8_t value = rleData[i + 1];
        for (uint8_t j = 0; j < count; j++) {
            result.push_back(value);
        }
    }

    return result;
}

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// A. Coordinate Conversion — latLonToTile
// ============================================================================

void test_latLonToTile_chicago(void) {
    uint32_t tx, ty;
    latLonToTile(41.8781, -87.6298, 14, tx, ty);
    TEST_ASSERT_EQUAL_UINT32(4203, tx);
    TEST_ASSERT_EQUAL_UINT32(6089, ty);
}

void test_latLonToTile_origin(void) {
    uint32_t tx, ty;
    latLonToTile(0.0, 0.0, 10, tx, ty);
    TEST_ASSERT_EQUAL_UINT32(512, tx);
    TEST_ASSERT_EQUAL_UINT32(512, ty);
}

void test_latLonToTile_new_york(void) {
    uint32_t tx, ty;
    latLonToTile(40.7128, -74.0060, 12, tx, ty);
    TEST_ASSERT_EQUAL_UINT32(1205, tx);
    TEST_ASSERT_EQUAL_UINT32(1540, ty);
}

void test_latLonToTile_london(void) {
    uint32_t tx, ty;
    latLonToTile(51.5074, -0.1278, 10, tx, ty);
    TEST_ASSERT_EQUAL_UINT32(511, tx);
    TEST_ASSERT_EQUAL_UINT32(340, ty);
}

void test_latLonToTile_sydney(void) {
    uint32_t tx, ty;
    latLonToTile(-33.8688, 151.2093, 12, tx, ty);
    TEST_ASSERT_EQUAL_UINT32(3768, tx);
    TEST_ASSERT_EQUAL_UINT32(2457, ty);
}

// ============================================================================
// B. Coordinate Conversion — tileToLatLon
// ============================================================================

void test_tileToLatLon_chicago_inverse(void) {
    double lat, lon;
    tileToLatLon(4203, 6092, 14, lat, lon);
    // Tile corner should be near Chicago
    TEST_ASSERT_FLOAT_WITHIN(0.05, 41.8781, lat);
    TEST_ASSERT_FLOAT_WITHIN(0.05, -87.6298, lon);
}

void test_tileToLatLon_origin(void) {
    double lat, lon;
    tileToLatLon(512, 512, 10, lat, lon);
    TEST_ASSERT_FLOAT_WITHIN(0.2, 0.0, lat);
    TEST_ASSERT_FLOAT_WITHIN(0.4, 0.0, lon);
}

void test_latLonToTile_roundtrip(void) {
    // Round-trip: latLonToTile → tileToLatLon → latLonToTile gives same tile
    double origLat = 48.8566, origLon = 2.3522; // Paris
    uint8_t z = 14;

    uint32_t tx1, ty1;
    latLonToTile(origLat, origLon, z, tx1, ty1);

    double midLat, midLon;
    tileToLatLon(tx1, ty1, z, midLat, midLon);

    uint32_t tx2, ty2;
    latLonToTile(midLat, midLon, z, tx2, ty2);

    TEST_ASSERT_EQUAL_UINT32(tx1, tx2);
    TEST_ASSERT_EQUAL_UINT32(ty1, ty2);
}

// ============================================================================
// C. Coordinate Conversion — latLonToPixel
// ============================================================================

void test_latLonToPixel_chicago(void) {
    int px, py;
    latLonToPixel(41.8781, -87.6298, 14, px, py);
    // Pixel should be approximately tileX * TILE_SIZE
    uint32_t tx, ty;
    latLonToTile(41.8781, -87.6298, 14, tx, ty);
    // pixelX / TILE_SIZE should equal tileX
    TEST_ASSERT_EQUAL_UINT32(tx, (uint32_t)(px / TILE_SIZE));
    TEST_ASSERT_EQUAL_UINT32(ty, (uint32_t)(py / TILE_SIZE));
}

void test_latLonToPixel_tile_consistency(void) {
    // pixel / TILE_SIZE == tileX,Y for any location
    double lat = 35.6762, lon = 139.6503; // Tokyo
    uint8_t z = 12;

    uint32_t tx, ty;
    latLonToTile(lat, lon, z, tx, ty);

    int px, py;
    latLonToPixel(lat, lon, z, px, py);

    TEST_ASSERT_EQUAL_UINT32(tx, (uint32_t)(px / TILE_SIZE));
    TEST_ASSERT_EQUAL_UINT32(ty, (uint32_t)(py / TILE_SIZE));
}

void test_latLonToPixel_deterministic(void) {
    int px1, py1, px2, py2;
    latLonToPixel(41.8781, -87.6298, 14, px1, py1);
    latLonToPixel(41.8781, -87.6298, 14, px2, py2);
    TEST_ASSERT_EQUAL_INT(px1, px2);
    TEST_ASSERT_EQUAL_INT(py1, py2);
}

// ============================================================================
// D. RLE Decompression
// ============================================================================

void test_rle_simple(void) {
    // {3, 0xAA, 2, 0x55} → 5 bytes: AA AA AA 55 55
    std::vector<uint8_t> input = {3, 0xAA, 2, 0x55};
    auto result = decompressRle(input);
    TEST_ASSERT_EQUAL(5, result.size());
    TEST_ASSERT_EQUAL(0xAA, result[0]);
    TEST_ASSERT_EQUAL(0xAA, result[1]);
    TEST_ASSERT_EQUAL(0xAA, result[2]);
    TEST_ASSERT_EQUAL(0x55, result[3]);
    TEST_ASSERT_EQUAL(0x55, result[4]);
}

void test_rle_all_white_tile(void) {
    // A full white tile: 2048 bytes of 0xFF
    // RLE: repeated runs of 255 x 0xFF until 2048 bytes
    // 2048 / 255 = 8 full runs (8*255=2040), then 1 run of 8
    std::vector<uint8_t> input;
    for (int i = 0; i < 8; i++) {
        input.push_back(255);
        input.push_back(0xFF);
    }
    input.push_back(8);
    input.push_back(0xFF);

    auto result = decompressRle(input);
    TEST_ASSERT_EQUAL(2048, result.size());
    for (size_t i = 0; i < result.size(); i++) {
        TEST_ASSERT_EQUAL(0xFF, result[i]);
    }
}

void test_rle_all_black_tile(void) {
    // A full black tile: 2048 bytes of 0x00
    std::vector<uint8_t> input;
    for (int i = 0; i < 8; i++) {
        input.push_back(255);
        input.push_back(0x00);
    }
    input.push_back(8);
    input.push_back(0x00);

    auto result = decompressRle(input);
    TEST_ASSERT_EQUAL(2048, result.size());
    for (size_t i = 0; i < result.size(); i++) {
        TEST_ASSERT_EQUAL(0x00, result[i]);
    }
}

void test_rle_alternating(void) {
    // Alternating: {1, 0xFF, 1, 0x00, 1, 0xFF, 1, 0x00}
    std::vector<uint8_t> input = {1, 0xFF, 1, 0x00, 1, 0xFF, 1, 0x00};
    auto result = decompressRle(input);
    TEST_ASSERT_EQUAL(4, result.size());
    TEST_ASSERT_EQUAL(0xFF, result[0]);
    TEST_ASSERT_EQUAL(0x00, result[1]);
    TEST_ASSERT_EQUAL(0xFF, result[2]);
    TEST_ASSERT_EQUAL(0x00, result[3]);
}

void test_rle_empty_input(void) {
    std::vector<uint8_t> input;
    auto result = decompressRle(input);
    TEST_ASSERT_EQUAL(0, result.size());
}

void test_rle_single_byte_odd(void) {
    // Single byte (odd length) → loop condition (i + 1 < 1) prevents reading
    std::vector<uint8_t> input = {0x42};
    auto result = decompressRle(input);
    TEST_ASSERT_EQUAL(0, result.size());
}

void test_rle_realistic_full_tile(void) {
    // Construct RLE that decompresses to exactly 2048 bytes
    // Pattern: 100 x 0xFF, 100 x 0x00, repeated until 2048
    std::vector<uint8_t> input;
    int total = 0;
    while (total < 2048) {
        int remaining = 2048 - total;
        int chunk = std::min(100, remaining);
        input.push_back((uint8_t)chunk);
        input.push_back((total / 100) % 2 == 0 ? 0xFF : 0x00);
        total += chunk;
    }

    auto result = decompressRle(input);
    TEST_ASSERT_EQUAL(2048, result.size());
}

// ============================================================================
// E. GPS Event Encoding/Decoding Round-trip
// ============================================================================

void test_gps_encoding_positive_coords(void) {
    // Chicago: 41.8781, -87.6298
    double origLat = 41.8781, origLon = -87.6298;

    // Encode as PositionService does
    uint32_t args0 = (uint32_t)(int32_t)(origLat * 1e7);
    uint32_t args1 = (uint32_t)(int32_t)(origLon * 1e7);

    // Decode as Maps::onEvent does
    double decodedLat = ((int32_t)args0) / 1e7;
    double decodedLon = ((int32_t)args1) / 1e7;

    TEST_ASSERT_FLOAT_WITHIN(0.0001, origLat, decodedLat);
    TEST_ASSERT_FLOAT_WITHIN(0.0001, origLon, decodedLon);
}

void test_gps_encoding_negative_coords(void) {
    // Sydney: -33.8688, 151.2093
    double origLat = -33.8688, origLon = 151.2093;

    uint32_t args0 = (uint32_t)(int32_t)(origLat * 1e7);
    uint32_t args1 = (uint32_t)(int32_t)(origLon * 1e7);

    double decodedLat = ((int32_t)args0) / 1e7;
    double decodedLon = ((int32_t)args1) / 1e7;

    TEST_ASSERT_FLOAT_WITHIN(0.0001, origLat, decodedLat);
    TEST_ASSERT_FLOAT_WITHIN(0.0001, origLon, decodedLon);
}

void test_gps_heading_encoding(void) {
    // Heading: 127.55° → args[2] = (uint32_t)(127.55 * 100) = 12755
    float origHeading = 127.55f;
    uint32_t args2 = (uint32_t)(origHeading * 100);

    float decodedHeading = args2 / 100.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01, origHeading, decodedHeading);
}

void test_gps_no_heading(void) {
    // args[3] = 0 means no heading
    uint32_t args3 = 0;
    bool hasHeading = (args3 != 0);
    TEST_ASSERT_FALSE(hasHeading);

    // args[3] = 1 means heading is valid
    args3 = 1;
    hasHeading = (args3 != 0);
    TEST_ASSERT_TRUE(hasHeading);
}

// ============================================================================
// F. Heading Rotation Math
// ============================================================================

// Triangle vertices relative to center: tip(0,-8), left-tail(-5,6), right-tail(5,6)
// Rotation: x' = x*cos - y*sin, y' = x*sin + y*cos

struct Pt { float x, y; };

static void rotateTriangle(float headingDeg, Pt& tip, Pt& left, Pt& right) {
    float rad = headingDeg * (float)M_PI / 180.0f;
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    // Original vertices
    float tipX = 0, tipY = -8;
    float leftX = -5, leftY = 6;
    float rightX = 5, rightY = 6;

    tip.x   = tipX * cosA - tipY * sinA;
    tip.y   = tipX * sinA + tipY * cosA;
    left.x  = leftX * cosA - leftY * sinA;
    left.y  = leftX * sinA + leftY * cosA;
    right.x = rightX * cosA - rightY * sinA;
    right.y = rightX * sinA + rightY * cosA;
}

void test_heading_north(void) {
    Pt tip, left, right;
    rotateTriangle(0.0f, tip, left, right);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, tip.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -8.0f, tip.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -5.0f, left.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 6.0f, left.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, right.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 6.0f, right.y);
}

void test_heading_east(void) {
    Pt tip, left, right;
    rotateTriangle(90.0f, tip, left, right);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8.0f, tip.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, tip.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -6.0f, left.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -5.0f, left.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -6.0f, right.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, right.y);
}

void test_heading_south(void) {
    Pt tip, left, right;
    rotateTriangle(180.0f, tip, left, right);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, tip.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8.0f, tip.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, left.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -6.0f, left.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -5.0f, right.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -6.0f, right.y);
}

void test_heading_west(void) {
    Pt tip, left, right;
    rotateTriangle(270.0f, tip, left, right);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -8.0f, tip.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, tip.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 6.0f, left.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, left.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 6.0f, right.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -5.0f, right.y);
}

void test_heading_45_degrees(void) {
    Pt tip, left, right;
    rotateTriangle(45.0f, tip, left, right);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.66f, tip.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -5.66f, tip.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -7.78f, left.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.71f, left.y);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -0.71f, right.x);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 7.78f, right.y);
}

// ============================================================================
// G. TileKey Operators
// ============================================================================

void test_tilekey_equal(void) {
    TileKey a = {14, 4203, 6092};
    TileKey b = {14, 4203, 6092};
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_FALSE(a != b);
}

void test_tilekey_not_equal_z(void) {
    TileKey a = {14, 4203, 6092};
    TileKey b = {15, 4203, 6092};
    TEST_ASSERT_FALSE(a == b);
    TEST_ASSERT_TRUE(a != b);
}

void test_tilekey_not_equal_x(void) {
    TileKey a = {14, 4203, 6092};
    TileKey b = {14, 4204, 6092};
    TEST_ASSERT_FALSE(a == b);
}

void test_tilekey_less_than_z_first(void) {
    TileKey a = {13, 9999, 9999};
    TileKey b = {14, 0, 0};
    TEST_ASSERT_TRUE(a < b);
    TEST_ASSERT_FALSE(b < a);
}

void test_tilekey_less_than_x_then_y(void) {
    TileKey a = {14, 100, 200};
    TileKey b = {14, 100, 201};
    TEST_ASSERT_TRUE(a < b);
    TEST_ASSERT_FALSE(b < a);

    TileKey c = {14, 99, 999};
    TileKey d = {14, 100, 0};
    TEST_ASSERT_TRUE(c < d);
    TEST_ASSERT_FALSE(d < c);
}

// ============================================================================
// H. Zoom Bounds Clamping
// ============================================================================

static int clampedZoom(int currentZoom, int delta) {
    int newZoom = currentZoom + delta;
    if (newZoom < MIN_ZOOM) newZoom = MIN_ZOOM;
    if (newZoom > MAX_ZOOM) newZoom = MAX_ZOOM;
    return newZoom;
}

void test_zoom_at_min_cannot_decrease(void) {
    TEST_ASSERT_EQUAL(MIN_ZOOM, clampedZoom(MIN_ZOOM, -1));
}

void test_zoom_at_max_cannot_increase(void) {
    TEST_ASSERT_EQUAL(MAX_ZOOM, clampedZoom(MAX_ZOOM, 1));
}

void test_zoom_normal_increase(void) {
    TEST_ASSERT_EQUAL(15, clampedZoom(14, 1));
}

void test_zoom_below_minimum_clamps(void) {
    TEST_ASSERT_EQUAL(MIN_ZOOM, clampedZoom(5, 0));
}

// ============================================================================
// I. Direction Instruction Formatting
// ============================================================================

// Reimplementation of Maps::populateDirections text formatting logic.
// Tests the maneuver→action mapping and instruction string generation
// identically to Maps.cpp, but free of LVGL dependencies.

static const char* maneuverToAction(const std::string& m) {
    if (m == "start" || m == "start-right" || m == "start-left") {
        return "Head";
    } else if (m == "turn-left") {
        return "Turn left";
    } else if (m == "turn-right") {
        return "Turn right";
    } else if (m == "turn-slight-left") {
        return "Slight left";
    } else if (m == "turn-slight-right") {
        return "Slight right";
    } else if (m == "turn-sharp-left") {
        return "Sharp left";
    } else if (m == "turn-sharp-right") {
        return "Sharp right";
    } else if (m == "u-turn-left" || m == "u-turn-right") {
        return "U-turn";
    } else if (m == "continue" || m == "straight") {
        return "Continue";
    } else if (m == "stay-straight") {
        return "Stay straight";
    } else if (m == "stay-left") {
        return "Stay left";
    } else if (m == "stay-right") {
        return "Stay right";
    } else if (m == "ramp-straight") {
        return "Take ramp";
    } else if (m == "ramp-left") {
        return "Ramp left";
    } else if (m == "ramp-right") {
        return "Ramp right";
    } else if (m == "exit-left") {
        return "Exit left";
    } else if (m == "exit-right") {
        return "Exit right";
    } else if (m == "merge") {
        return "Merge";
    } else if (m == "roundabout-enter") {
        return "Enter roundabout";
    } else if (m == "roundabout-exit") {
        return "Exit roundabout";
    } else if (m == "ferry-enter") {
        return "Take ferry";
    } else if (m == "ferry-exit") {
        return "Exit ferry";
    } else if (m == "destination" || m == "destination-left" || m == "destination-right") {
        return "Arrive";
    }
    return "Continue";
}

// Format the instruction text (without the LVGL icon prefix).
// Returns the formatted string identical to what Maps::populateDirections builds.
static std::string formatInstruction(const std::string& maneuver,
                                     const std::string& streetName,
                                     uint32_t distance_m) {
    const char* action = maneuverToAction(maneuver);

    std::string street = streetName;
    if (street.length() > 20) {
        street = street.substr(0, 17) + "...";
    }

    char dist[16];
    if (distance_m >= 1000) {
        snprintf(dist, sizeof(dist), "%.1fkm", distance_m / 1000.0f);
    } else {
        snprintf(dist, sizeof(dist), "%um", distance_m);
    }

    char text[160];
    if (!street.empty()) {
        snprintf(text, sizeof(text), "%s on %s (%s)", action, street.c_str(), dist);
    } else {
        snprintf(text, sizeof(text), "%s (%s)", action, dist);
    }
    return std::string(text);
}

// --- Maneuver-to-action mapping tests ---

void test_maneuver_turn_left(void) {
    TEST_ASSERT_EQUAL_STRING("Turn left", maneuverToAction("turn-left"));
}

void test_maneuver_turn_right(void) {
    TEST_ASSERT_EQUAL_STRING("Turn right", maneuverToAction("turn-right"));
}

void test_maneuver_slight_turns(void) {
    TEST_ASSERT_EQUAL_STRING("Slight left", maneuverToAction("turn-slight-left"));
    TEST_ASSERT_EQUAL_STRING("Slight right", maneuverToAction("turn-slight-right"));
}

void test_maneuver_sharp_turns(void) {
    TEST_ASSERT_EQUAL_STRING("Sharp left", maneuverToAction("turn-sharp-left"));
    TEST_ASSERT_EQUAL_STRING("Sharp right", maneuverToAction("turn-sharp-right"));
}

void test_maneuver_u_turns(void) {
    TEST_ASSERT_EQUAL_STRING("U-turn", maneuverToAction("u-turn-left"));
    TEST_ASSERT_EQUAL_STRING("U-turn", maneuverToAction("u-turn-right"));
}

void test_maneuver_start_variants(void) {
    TEST_ASSERT_EQUAL_STRING("Head", maneuverToAction("start"));
    TEST_ASSERT_EQUAL_STRING("Head", maneuverToAction("start-left"));
    TEST_ASSERT_EQUAL_STRING("Head", maneuverToAction("start-right"));
}

void test_maneuver_destination_variants(void) {
    TEST_ASSERT_EQUAL_STRING("Arrive", maneuverToAction("destination"));
    TEST_ASSERT_EQUAL_STRING("Arrive", maneuverToAction("destination-left"));
    TEST_ASSERT_EQUAL_STRING("Arrive", maneuverToAction("destination-right"));
}

void test_maneuver_continue_variants(void) {
    TEST_ASSERT_EQUAL_STRING("Continue", maneuverToAction("continue"));
    TEST_ASSERT_EQUAL_STRING("Continue", maneuverToAction("straight"));
}

void test_maneuver_stay_variants(void) {
    TEST_ASSERT_EQUAL_STRING("Stay straight", maneuverToAction("stay-straight"));
    TEST_ASSERT_EQUAL_STRING("Stay left", maneuverToAction("stay-left"));
    TEST_ASSERT_EQUAL_STRING("Stay right", maneuverToAction("stay-right"));
}

void test_maneuver_ramp_variants(void) {
    TEST_ASSERT_EQUAL_STRING("Take ramp", maneuverToAction("ramp-straight"));
    TEST_ASSERT_EQUAL_STRING("Ramp left", maneuverToAction("ramp-left"));
    TEST_ASSERT_EQUAL_STRING("Ramp right", maneuverToAction("ramp-right"));
}

void test_maneuver_exit_variants(void) {
    TEST_ASSERT_EQUAL_STRING("Exit left", maneuverToAction("exit-left"));
    TEST_ASSERT_EQUAL_STRING("Exit right", maneuverToAction("exit-right"));
}

void test_maneuver_merge(void) {
    TEST_ASSERT_EQUAL_STRING("Merge", maneuverToAction("merge"));
}

void test_maneuver_roundabout(void) {
    TEST_ASSERT_EQUAL_STRING("Enter roundabout", maneuverToAction("roundabout-enter"));
    TEST_ASSERT_EQUAL_STRING("Exit roundabout", maneuverToAction("roundabout-exit"));
}

void test_maneuver_ferry(void) {
    TEST_ASSERT_EQUAL_STRING("Take ferry", maneuverToAction("ferry-enter"));
    TEST_ASSERT_EQUAL_STRING("Exit ferry", maneuverToAction("ferry-exit"));
}

void test_maneuver_unknown_defaults_to_continue(void) {
    TEST_ASSERT_EQUAL_STRING("Continue", maneuverToAction(""));
    TEST_ASSERT_EQUAL_STRING("Continue", maneuverToAction("none"));
    TEST_ASSERT_EQUAL_STRING("Continue", maneuverToAction("becomes"));
    TEST_ASSERT_EQUAL_STRING("Continue", maneuverToAction("something-unexpected"));
}

// --- Full instruction formatting tests ---

void test_format_with_street_and_meters(void) {
    std::string result = formatInstruction("turn-left", "Main Street", 250);
    TEST_ASSERT_EQUAL_STRING("Turn left on Main Street (250m)", result.c_str());
}

void test_format_with_street_and_km(void) {
    std::string result = formatInstruction("turn-right", "Oak Avenue", 1500);
    TEST_ASSERT_EQUAL_STRING("Turn right on Oak Avenue (1.5km)", result.c_str());
}

void test_format_without_street(void) {
    std::string result = formatInstruction("turn-left", "", 300);
    TEST_ASSERT_EQUAL_STRING("Turn left (300m)", result.c_str());
}

void test_format_start_no_street(void) {
    std::string result = formatInstruction("start-left", "", 0);
    TEST_ASSERT_EQUAL_STRING("Head (0m)", result.c_str());
}

void test_format_destination(void) {
    std::string result = formatInstruction("destination", "123 Elm St", 0);
    TEST_ASSERT_EQUAL_STRING("Arrive on 123 Elm St (0m)", result.c_str());
}

void test_format_long_street_truncated(void) {
    std::string result = formatInstruction("continue", "North Michigan Avenue Boulevard", 800);
    // 31 chars > 20, truncated to 17 + "..."
    TEST_ASSERT_EQUAL_STRING("Continue on North Michigan Av... (800m)", result.c_str());
}

void test_format_street_exactly_20_chars(void) {
    // Exactly 20 chars — should NOT be truncated
    std::string street = "12345678901234567890";
    TEST_ASSERT_EQUAL(20, street.length());
    std::string result = formatInstruction("turn-right", street, 100);
    TEST_ASSERT_EQUAL_STRING("Turn right on 12345678901234567890 (100m)", result.c_str());
}

void test_format_street_21_chars_truncated(void) {
    // 21 chars — should be truncated to 17 + "..."
    std::string street = "123456789012345678901";
    TEST_ASSERT_EQUAL(21, street.length());
    std::string result = formatInstruction("turn-left", street, 500);
    TEST_ASSERT_EQUAL_STRING("Turn left on 12345678901234567... (500m)", result.c_str());
}

void test_format_roundabout_with_street(void) {
    std::string result = formatInstruction("roundabout-enter", "Circle Drive", 80);
    TEST_ASSERT_EQUAL_STRING("Enter roundabout on Circle Drive (80m)", result.c_str());
}

void test_format_km_boundary(void) {
    // Exactly 1000m should show as km
    std::string result = formatInstruction("continue", "", 1000);
    TEST_ASSERT_EQUAL_STRING("Continue (1.0km)", result.c_str());
}

void test_format_999m_stays_meters(void) {
    std::string result = formatInstruction("continue", "", 999);
    TEST_ASSERT_EQUAL_STRING("Continue (999m)", result.c_str());
}

void test_format_large_distance(void) {
    std::string result = formatInstruction("merge", "I-90", 12500);
    TEST_ASSERT_EQUAL_STRING("Merge on I-90 (12.5km)", result.c_str());
}


// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // A. latLonToTile
    RUN_TEST(test_latLonToTile_chicago);
    RUN_TEST(test_latLonToTile_origin);
    RUN_TEST(test_latLonToTile_new_york);
    RUN_TEST(test_latLonToTile_london);
    RUN_TEST(test_latLonToTile_sydney);

    // B. tileToLatLon
    RUN_TEST(test_tileToLatLon_chicago_inverse);
    RUN_TEST(test_tileToLatLon_origin);
    RUN_TEST(test_latLonToTile_roundtrip);

    // C. latLonToPixel
    RUN_TEST(test_latLonToPixel_chicago);
    RUN_TEST(test_latLonToPixel_tile_consistency);
    RUN_TEST(test_latLonToPixel_deterministic);

    // D. RLE Decompression
    RUN_TEST(test_rle_simple);
    RUN_TEST(test_rle_all_white_tile);
    RUN_TEST(test_rle_all_black_tile);
    RUN_TEST(test_rle_alternating);
    RUN_TEST(test_rle_empty_input);
    RUN_TEST(test_rle_single_byte_odd);
    RUN_TEST(test_rle_realistic_full_tile);

    // E. GPS Event Encoding
    RUN_TEST(test_gps_encoding_positive_coords);
    RUN_TEST(test_gps_encoding_negative_coords);
    RUN_TEST(test_gps_heading_encoding);
    RUN_TEST(test_gps_no_heading);

    // F. Heading Rotation
    RUN_TEST(test_heading_north);
    RUN_TEST(test_heading_east);
    RUN_TEST(test_heading_south);
    RUN_TEST(test_heading_west);
    RUN_TEST(test_heading_45_degrees);

    // G. TileKey Operators
    RUN_TEST(test_tilekey_equal);
    RUN_TEST(test_tilekey_not_equal_z);
    RUN_TEST(test_tilekey_not_equal_x);
    RUN_TEST(test_tilekey_less_than_z_first);
    RUN_TEST(test_tilekey_less_than_x_then_y);

    // H. Zoom Bounds
    RUN_TEST(test_zoom_at_min_cannot_decrease);
    RUN_TEST(test_zoom_at_max_cannot_increase);
    RUN_TEST(test_zoom_normal_increase);
    RUN_TEST(test_zoom_below_minimum_clamps);

    // I. Direction Instruction Formatting — maneuver mapping
    RUN_TEST(test_maneuver_turn_left);
    RUN_TEST(test_maneuver_turn_right);
    RUN_TEST(test_maneuver_slight_turns);
    RUN_TEST(test_maneuver_sharp_turns);
    RUN_TEST(test_maneuver_u_turns);
    RUN_TEST(test_maneuver_start_variants);
    RUN_TEST(test_maneuver_destination_variants);
    RUN_TEST(test_maneuver_continue_variants);
    RUN_TEST(test_maneuver_stay_variants);
    RUN_TEST(test_maneuver_ramp_variants);
    RUN_TEST(test_maneuver_exit_variants);
    RUN_TEST(test_maneuver_merge);
    RUN_TEST(test_maneuver_roundabout);
    RUN_TEST(test_maneuver_ferry);
    RUN_TEST(test_maneuver_unknown_defaults_to_continue);

    // I. Direction Instruction Formatting — full text output
    RUN_TEST(test_format_with_street_and_meters);
    RUN_TEST(test_format_with_street_and_km);
    RUN_TEST(test_format_without_street);
    RUN_TEST(test_format_start_no_street);
    RUN_TEST(test_format_destination);
    RUN_TEST(test_format_long_street_truncated);
    RUN_TEST(test_format_street_exactly_20_chars);
    RUN_TEST(test_format_street_21_chars_truncated);
    RUN_TEST(test_format_roundabout_with_street);
    RUN_TEST(test_format_km_boundary);
    RUN_TEST(test_format_999m_stays_meters);
    RUN_TEST(test_format_large_distance);

    return UNITY_END();
}
