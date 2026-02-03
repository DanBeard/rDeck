/**
 * Unit tests for TrustedServers persistence and management.
 */

#include <unity.h>
#include <ArduinoJson.h>
#include "services/RnsUtils/TrustedServers.h"
#include "Bytes.h"

// Use the emulator's FS implementation
#include "FS.h"

using namespace Retcon::Service;

// Test directory path - use a temp directory for isolation
static std::string testDir;
static FS* testFs = nullptr;
static TrustedServers servers;

void setUp(void) {
    // Create a unique test directory
    char dirTemplate[] = "/tmp/test_trusted_servers_XXXXXX";
    char* result = mkdtemp(dirTemplate);
    if (result) {
        testDir = result;
    } else {
        testDir = "/tmp/test_trusted_servers";
        mkdir(testDir.c_str(), 0755);
    }

    // Create FS pointing to test directory
    if (testFs) {
        delete testFs;
    }
    testFs = new FS(testDir.c_str());

    // Reset to fresh instance
    servers = TrustedServers();
    servers.init(testFs);
}

void tearDown(void) {
    // Clean up test files
    std::string filePath = testDir + "/trusted_servers.json";
    remove(filePath.c_str());
    rmdir(testDir.c_str());
}

// ============================================================================
// TrustedServer struct tests
// ============================================================================

void test_trusted_server_serialize(void) {
    TrustedServer server;
    server.hash.assignHex("abcd1234");
    server.name = "Test Server";
    server.services.push_back("ntp");
    server.services.push_back("search");
    server.status = TrustStatus::TRUSTED;
    server.offered_at = 1000;
    server.accepted_at = 2000;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    server.serialize(obj);

    TEST_ASSERT_EQUAL_STRING("Test Server", obj["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("trusted", obj["status"].as<const char*>());
    TEST_ASSERT_EQUAL(1000, obj["offered_at"].as<long>());
    TEST_ASSERT_EQUAL(2000, obj["accepted_at"].as<long>());
    TEST_ASSERT_TRUE(obj["services"].is<JsonArray>());
    TEST_ASSERT_EQUAL(2, obj["services"].size());
}

void test_trusted_server_deserialize(void) {
    JsonDocument doc;
    doc["name"] = "Another Server";
    doc["status"] = "pending";
    doc["offered_at"] = 500;
    doc["accepted_at"] = 0;
    JsonArray svc = doc["services"].to<JsonArray>();
    svc.add("ntp");

    TrustedServer server;
    JsonObject obj = doc.as<JsonObject>();
    server.deserialize(obj);

    TEST_ASSERT_EQUAL_STRING("Another Server", server.name.c_str());
    TEST_ASSERT_EQUAL(TrustStatus::PENDING, server.status);
    TEST_ASSERT_EQUAL(500, server.offered_at);
    TEST_ASSERT_EQUAL(1, server.services.size());
}

void test_trusted_server_hash_hex(void) {
    TrustedServer server;
    server.hash.assignHex("deadbeef");

    TEST_ASSERT_EQUAL_STRING("deadbeef", server.hashHex().c_str());
}

// ============================================================================
// TrustedServers basic operations
// ============================================================================

void test_empty_initially(void) {
    auto all = servers.getAllServers();
    TEST_ASSERT_EQUAL(0, all.size());
}

void test_add_pending_offer(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp", "search"};
    servers.addPendingOffer(hash, "Test Server", svc);

    auto all = servers.getAllServers();
    TEST_ASSERT_EQUAL(1, all.size());
    TEST_ASSERT_EQUAL_STRING("Test Server", all[0].name.c_str());
    TEST_ASSERT_EQUAL(TrustStatus::PENDING, all[0].status);
}

void test_has_pending_offer(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash, "Server", svc);

    TEST_ASSERT_TRUE(servers.hasPendingOffer("abcd1234abcd1234"));
    TEST_ASSERT_FALSE(servers.hasPendingOffer("deadbeef"));
    TEST_ASSERT_FALSE(servers.isTrusted(std::string("abcd1234abcd1234")));
}

void test_accept_offer(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash, "Server", svc);

    bool accepted = servers.acceptOffer("abcd1234abcd1234");

    TEST_ASSERT_TRUE(accepted);
    TEST_ASSERT_TRUE(servers.isTrusted(std::string("abcd1234abcd1234")));
    TEST_ASSERT_FALSE(servers.hasPendingOffer("abcd1234abcd1234"));
}

void test_accept_nonexistent_returns_false(void) {
    bool accepted = servers.acceptOffer("nonexistent");
    TEST_ASSERT_FALSE(accepted);
}

void test_revoke_trust(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash, "Server", svc);
    servers.acceptOffer("abcd1234abcd1234");

    TEST_ASSERT_TRUE(servers.isTrusted(std::string("abcd1234abcd1234")));

    servers.revokeTrust("abcd1234abcd1234");

    TEST_ASSERT_FALSE(servers.isTrusted(std::string("abcd1234abcd1234")));
    TEST_ASSERT_EQUAL(nullptr, servers.getServer("abcd1234abcd1234"));
}

void test_is_trusted_with_bytes(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash, "Server", svc);
    servers.acceptOffer("abcd1234abcd1234");

    TEST_ASSERT_TRUE(servers.isTrusted(hash));
}

void test_get_server(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp", "search"};
    servers.addPendingOffer(hash, "Test Server", svc);

    const TrustedServer* server = servers.getServer("abcd1234abcd1234");

    TEST_ASSERT_NOT_NULL(server);
    TEST_ASSERT_EQUAL_STRING("Test Server", server->name.c_str());
    TEST_ASSERT_EQUAL(2, server->services.size());
}

void test_get_pending_offers(void) {
    RNS::Bytes hash1, hash2, hash3;
    hash1.assignHex("1111111111111111");
    hash2.assignHex("2222222222222222");
    hash3.assignHex("3333333333333333");

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash1, "Server1", svc);
    servers.addPendingOffer(hash2, "Server2", svc);
    servers.addPendingOffer(hash3, "Server3", svc);

    servers.acceptOffer("1111111111111111");
    servers.acceptOffer("3333333333333333");

    auto pending = servers.getPendingOffers();
    TEST_ASSERT_EQUAL(1, pending.size());
    TEST_ASSERT_EQUAL_STRING("Server2", pending[0].name.c_str());
}

void test_get_trusted_servers(void) {
    RNS::Bytes hash1, hash2;
    hash1.assignHex("1111111111111111");
    hash2.assignHex("2222222222222222");

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash1, "Server1", svc);
    servers.addPendingOffer(hash2, "Server2", svc);

    servers.acceptOffer("1111111111111111");

    auto trusted = servers.getTrustedServers();
    TEST_ASSERT_EQUAL(1, trusted.size());
    TEST_ASSERT_EQUAL_STRING("Server1", trusted[0].name.c_str());
}

// ============================================================================
// Persistence tests
// ============================================================================

void test_persistence_saves_file(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash, "Server", svc);

    // File should exist after adding
    std::string filePath = testDir + "/trusted_servers.json";
    struct stat st;
    TEST_ASSERT_EQUAL(0, stat(filePath.c_str(), &st));
}

void test_persistence_file_format(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp", "search"};
    servers.addPendingOffer(hash, "Test Server", svc);
    servers.acceptOffer("abcd1234abcd1234");

    // Read and parse the file
    std::string filePath = testDir + "/trusted_servers.json";
    FILE* f = fopen(filePath.c_str(), "r");
    TEST_ASSERT_NOT_NULL(f);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string content;
    content.resize(size);
    fread(&content[0], 1, size, f);
    fclose(f);

    JsonDocument doc;
    deserializeJson(doc, content);

    TEST_ASSERT_EQUAL(TrustedServers::SCHEMA_VERSION, doc["version"].as<int>());
    TEST_ASSERT_TRUE(doc["servers"].is<JsonObject>());
    TEST_ASSERT_TRUE(doc["servers"]["abcd1234abcd1234"].is<JsonObject>());
    TEST_ASSERT_EQUAL_STRING("trusted", doc["servers"]["abcd1234abcd1234"]["status"].as<const char*>());
}

void test_persistence_reload(void) {
    // First, add and accept a server
    RNS::Bytes hash;
    hash.assignHex("fedcba9876543210");  // Valid hex string

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash, "Persistent Server", svc);
    servers.acceptOffer("fedcba9876543210");

    // Create a new TrustedServers instance and load from same fs
    TrustedServers newServers;
    newServers.init(testFs);

    // Should have the same trusted server
    TEST_ASSERT_TRUE(newServers.isTrusted(std::string("fedcba9876543210")));
    const TrustedServer* server = newServers.getServer("fedcba9876543210");
    TEST_ASSERT_NOT_NULL(server);
    TEST_ASSERT_EQUAL_STRING("Persistent Server", server->name.c_str());
}

// ============================================================================
// Edge cases
// ============================================================================

void test_offer_does_not_overwrite_trusted(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp"};
    servers.addPendingOffer(hash, "Server", svc);
    servers.acceptOffer("abcd1234abcd1234");

    TEST_ASSERT_TRUE(servers.isTrusted(std::string("abcd1234abcd1234")));

    // Try to re-offer
    servers.addPendingOffer(hash, "New Name", svc);

    // Should still be trusted
    TEST_ASSERT_TRUE(servers.isTrusted(std::string("abcd1234abcd1234")));
}

void test_revoke_nonexistent_does_not_crash(void) {
    // Should not crash
    servers.revokeTrust("nonexistent");
    TEST_ASSERT_TRUE(true);
}

void test_multiple_services(void) {
    RNS::Bytes hash;
    hash.assignHex("abcd1234abcd1234");

    std::vector<std::string> svc = {"ntp", "search", "email", "custom"};
    servers.addPendingOffer(hash, "Multi-Service Server", svc);

    const TrustedServer* server = servers.getServer("abcd1234abcd1234");
    TEST_ASSERT_NOT_NULL(server);
    TEST_ASSERT_EQUAL(4, server->services.size());
    TEST_ASSERT_EQUAL_STRING("ntp", server->services[0].c_str());
    TEST_ASSERT_EQUAL_STRING("custom", server->services[3].c_str());
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // TrustedServer struct tests
    RUN_TEST(test_trusted_server_serialize);
    RUN_TEST(test_trusted_server_deserialize);
    RUN_TEST(test_trusted_server_hash_hex);

    // Basic operations
    RUN_TEST(test_empty_initially);
    RUN_TEST(test_add_pending_offer);
    RUN_TEST(test_has_pending_offer);
    RUN_TEST(test_accept_offer);
    RUN_TEST(test_accept_nonexistent_returns_false);
    RUN_TEST(test_revoke_trust);
    RUN_TEST(test_is_trusted_with_bytes);
    RUN_TEST(test_get_server);
    RUN_TEST(test_get_pending_offers);
    RUN_TEST(test_get_trusted_servers);

    // Persistence tests
    RUN_TEST(test_persistence_saves_file);
    RUN_TEST(test_persistence_file_format);
    RUN_TEST(test_persistence_reload);

    // Edge cases
    RUN_TEST(test_offer_does_not_overwrite_trusted);
    RUN_TEST(test_revoke_nonexistent_does_not_crash);
    RUN_TEST(test_multiple_services);

    // Cleanup
    if (testFs) {
        delete testFs;
        testFs = nullptr;
    }

    return UNITY_END();
}
