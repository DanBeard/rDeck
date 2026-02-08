/**
 * Integration Tests for Settings App Components
 *
 * These tests verify the components used by the Settings app that could cause
 * crashes, without requiring full LVGL/SDL2 initialization.
 *
 * Tests include:
 * 1. JSON settings loading and parsing
 * 2. TrustedServers data access (used by drawTrustedServersSection)
 * 3. LoraConfig access patterns (used by RnsService::drawSettings)
 * 4. Service iteration (used when drawing service settings)
 */

#include <unity.h>
#include <ArduinoJson.h>
#include <string>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include "services/RnsUtils/TrustedServers.h"
#include "FS.h"

// Test fixture
static char* testDir = nullptr;
static FS* testFs = nullptr;

void setUp(void) {
    static char dirBuf[64];
    strcpy(dirBuf, "/tmp/test_settings_XXXXXX");
    testDir = mkdtemp(dirBuf);
    testFs = new FS(testDir);
}

void tearDown(void) {
    if (testFs) {
        delete testFs;
        testFs = nullptr;
    }
    if (testDir) {
        // Clean up files
        std::string settingsPath = std::string(testDir) + "/settings.json";
        std::string trustedPath = std::string(testDir) + "/trusted_servers.json";
        remove(settingsPath.c_str());
        remove(trustedPath.c_str());
        rmdir(testDir);
        testDir = nullptr;
    }
}

// Helper to read file into string
static std::string readFileToString(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ============================================================================
// Settings JSON Tests
// ============================================================================

void test_settings_json_empty_file(void) {
    // Create empty settings file like Settings::getSettings does
    std::string path = std::string(testDir) + "/settings.json";
    std::ofstream outFile(path);
    outFile << "{}";
    outFile.close();

    // Read it back
    std::string content = readFileToString(path);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, content);

    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    TEST_ASSERT_TRUE(doc.is<JsonObject>());
}

void test_settings_json_with_sections(void) {
    // Create settings file with sections
    std::string path = std::string(testDir) + "/settings.json";
    std::ofstream outFile(path);
    outFile << R"({
        "settings": {
            "timezone": "PST8PDT",
            "epoch": 1706825600
        },
        "reticulum": {
            "lora": {
                "frequency": 915.0,
                "bandwidth": 125.0,
                "sf": 7,
                "cr": 5
            }
        }
    })";
    outFile.close();

    // Read and parse
    std::string content = readFileToString(path);
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, content);

    TEST_ASSERT_TRUE(err == DeserializationError::Ok);

    // Access settings section
    JsonObject settings = doc["settings"];
    TEST_ASSERT_FALSE(settings.isNull());
    TEST_ASSERT_EQUAL_STRING("PST8PDT", settings["timezone"].as<const char*>());

    // Access reticulum section
    JsonObject reticulum = doc["reticulum"];
    TEST_ASSERT_FALSE(reticulum.isNull());
    JsonObject lora = reticulum["lora"];
    TEST_ASSERT_FALSE(lora.isNull());
    TEST_ASSERT_EQUAL_FLOAT(915.0f, lora["frequency"].as<float>());
}

void test_settings_json_auto_create_section(void) {
    // Test auto-creating a section like Settings::getSettings does
    JsonDocument doc;

    // Simulate what Settings::getSettings does
    const char* section = "settings";
    if (!doc.containsKey(section)) {
        doc[section]["auto_created"] = true;
    }

    JsonObject settings = doc[section];
    TEST_ASSERT_FALSE(settings.isNull());
    TEST_ASSERT_TRUE(settings["auto_created"].as<bool>());
}

// ============================================================================
// LoraConfig Access Tests (used by RnsService::drawSettings)
// ============================================================================

// Minimal LoraConfig struct for testing
struct TestLoraConfig {
    float frequency = 0.0f;
    float bandwidth = 0.0f;
    int sf = 0;
    int cr = 0;
    int power = 0;
    int preamble_len = 0;
    int crc = 0;
    bool explicitHeader = false;
};

void test_lora_config_uninitialized_snprintf(void) {
    // Test that snprintf with uninitialized float values doesn't crash
    TestLoraConfig config;  // Zero-initialized

    char buf[32];
    int ret = snprintf(buf, sizeof(buf), "%.3f", config.frequency);
    TEST_ASSERT_GREATER_THAN(0, ret);
    TEST_ASSERT_EQUAL_STRING("0.000", buf);
}

void test_lora_config_default_values_snprintf(void) {
    // Test snprintf with default values like EmulatorLora::initLora sets
    TestLoraConfig config;
    config.frequency = 915.0f;
    config.bandwidth = 125.0f;
    config.sf = 7;
    config.cr = 5;

    char buf[32];

    snprintf(buf, sizeof(buf), "%.3f", config.frequency);
    TEST_ASSERT_EQUAL_STRING("915.000", buf);

    snprintf(buf, sizeof(buf), "%.3f", config.bandwidth);
    TEST_ASSERT_EQUAL_STRING("125.000", buf);

    // Test snprintf for integer values
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", config.sf);
    TEST_ASSERT_EQUAL_STRING("7", tmp);
}

void test_lora_config_nan_values_snprintf(void) {
    // Test snprintf with NaN values (potential crash source)
    // Note: We're testing that this doesn't crash, not that output is useful
    float nanValue = 0.0f / 0.0f;  // Create NaN

    char buf[32];
    int ret = snprintf(buf, sizeof(buf), "%.3f", nanValue);

    // Should not crash, may produce "nan" or similar
    TEST_ASSERT_GREATER_THAN(0, ret);
}

// ============================================================================
// TrustedServers Access Tests (used by drawTrustedServersSection)
// ============================================================================

void test_trusted_servers_empty_lists(void) {
    // Test accessing empty trusted servers (common case on fresh install)
    Retcon::Service::TrustedServers servers;
    servers.init(testFs);

    auto pending = servers.getPendingOffers();
    auto trusted = servers.getTrustedServers();

    TEST_ASSERT_EQUAL(0, pending.size());
    TEST_ASSERT_EQUAL(0, trusted.size());
}

void test_trusted_servers_with_pending_offer(void) {
    Retcon::Service::TrustedServers servers;
    servers.init(testFs);

    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");
    std::vector<std::string> svcs = {"ntp", "search"};
    servers.addPendingOffer(hash, "Test Server", svcs);

    auto pending = servers.getPendingOffers();
    TEST_ASSERT_EQUAL(1, pending.size());
    TEST_ASSERT_EQUAL_STRING("Test Server", pending[0].name.c_str());
    TEST_ASSERT_EQUAL(2, pending[0].services.size());
}

void test_trusted_servers_iteration(void) {
    // Test iterating over servers like drawTrustedServersSection does
    Retcon::Service::TrustedServers servers;
    servers.init(testFs);

    // Add multiple servers
    RNS::Bytes hash1, hash2;
    hash1.assignHex("1111111111111111");
    hash2.assignHex("2222222222222222");

    std::vector<std::string> svcs = {"ntp"};
    servers.addPendingOffer(hash1, "Server 1", svcs);
    servers.addPendingOffer(hash2, "Server 2", svcs);
    servers.acceptOffer("1111111111111111");

    auto pending = servers.getPendingOffers();
    auto trusted = servers.getTrustedServers();

    TEST_ASSERT_EQUAL(1, pending.size());
    TEST_ASSERT_EQUAL(1, trusted.size());

    // Iterate like UI code does
    for (const auto& server : pending) {
        TEST_ASSERT_FALSE(server.name.empty());
        TEST_ASSERT_FALSE(server.hashHex().empty());
    }
    for (const auto& server : trusted) {
        TEST_ASSERT_FALSE(server.name.empty());
        TEST_ASSERT_FALSE(server.hashHex().empty());
    }
}

// ============================================================================
// Service Info Iteration Tests
// ============================================================================

// Minimal ServiceInfo-like struct for testing the iteration pattern
struct TestServiceInfo {
    int id;
    bool (*drawSettings)();
    void (*applySettings)();
};

static bool testDrawSettingsCalled = false;
static bool testDrawSettings() {
    testDrawSettingsCalled = true;
    return true;
}

void test_service_info_iteration(void) {
    // Test iterating over service info like Settings::drawScreen does
    std::vector<TestServiceInfo> services = {
        {1, testDrawSettings, nullptr},
        {2, testDrawSettings, nullptr},
        {3, testDrawSettings, nullptr}
    };

    int count = 0;
    for (const auto& sInfo : services) {
        testDrawSettingsCalled = false;
        if (sInfo.drawSettings) {
            sInfo.drawSettings();
        }
        if (testDrawSettingsCalled) {
            count++;
        }
    }

    TEST_ASSERT_EQUAL(3, count);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Settings JSON tests
    RUN_TEST(test_settings_json_empty_file);
    RUN_TEST(test_settings_json_with_sections);
    RUN_TEST(test_settings_json_auto_create_section);

    // LoraConfig tests
    RUN_TEST(test_lora_config_uninitialized_snprintf);
    RUN_TEST(test_lora_config_default_values_snprintf);
    RUN_TEST(test_lora_config_nan_values_snprintf);

    // TrustedServers tests
    RUN_TEST(test_trusted_servers_empty_lists);
    RUN_TEST(test_trusted_servers_with_pending_offer);
    RUN_TEST(test_trusted_servers_iteration);

    // Service iteration tests
    RUN_TEST(test_service_info_iteration);

    return UNITY_END();
}
