/**
 * Unit tests for LXMF announce de-duplication
 *
 * Tests that announces are properly de-duplicated by destination address,
 * keeping only the most recent announce per destination.
 *
 * The implementation uses a map keyed by destination for O(log n) operations
 * and maintains a sorted set (by timestamp) for iteration.
 */

#include <unity.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

#include "FS.h"
#include "services/RnsUtils/LXMFData.h"

using namespace Retcon::LXMF;

// Test fixture state
static FS* test_fs = nullptr;
static char test_dir[256] = {0};

// Helper to create unique temp directory for each test
static void create_test_dir() {
    snprintf(test_dir, sizeof(test_dir), "/tmp/rdeck_announce_test_%d_%ld",
             getpid(), (long)time(nullptr));
    mkdir(test_dir, 0755);

    // Create subdirectories needed by LXMFData
    char subdir[512];
    snprintf(subdir, sizeof(subdir), "%s/lxmf", test_dir);
    mkdir(subdir, 0755);
    snprintf(subdir, sizeof(subdir), "%s/lxmf/conversations", test_dir);
    mkdir(subdir, 0755);
}

// Helper to recursively remove test directory
static void remove_test_dir() {
    if (test_dir[0] == '\0') return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);
    test_dir[0] = '\0';
}

// Helper to create test destination hash
static RNS::Bytes makeDest(uint8_t fill) {
    uint8_t data[16];
    memset(data, fill, 16);
    return RNS::Bytes(data, 16);
}

// Helper to create test app_data
static RNS::Bytes makeAppData(const char* name) {
    // Simple app_data - just the name bytes
    return RNS::Bytes((uint8_t*)name, strlen(name));
}

void setUp(void) {
    create_test_dir();
    test_fs = new FS(test_dir);
    setTestFilesystem(test_fs);
    resetAllState();
}

void tearDown(void) {
    resetAllState();
    setTestFilesystem(nullptr);
    delete test_fs;
    test_fs = nullptr;
    remove_test_dir();
}

// Test: Basic add and retrieve
void test_add_single_announce(void) {
    RNS::Bytes dest = makeDest(0x11);
    RNS::Bytes app_data = makeAppData("TestNode");
    time_t now = time(nullptr);

    AnnounceData announce(dest, app_data, now);
    addAnnounceData(announce);

    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL(1, announces->size());

    auto it = announces->begin();
    TEST_ASSERT_EQUAL(16, it->dest.size());
    TEST_ASSERT_EQUAL(0x11, it->dest.data()[0]);
    TEST_ASSERT_EQUAL(now, it->last_heard);
}

// Test: De-duplication - same destination replaces older announce
void test_dedup_same_destination_replaces_older(void) {
    RNS::Bytes dest = makeDest(0x22);
    RNS::Bytes app_data1 = makeAppData("OldName");
    RNS::Bytes app_data2 = makeAppData("NewName");
    time_t old_time = 1000;
    time_t new_time = 2000;

    // Add first announce
    AnnounceData announce1(dest, app_data1, old_time);
    addAnnounceData(announce1);

    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL(1, announces->size());
    TEST_ASSERT_EQUAL(old_time, announces->begin()->last_heard);

    // Add second announce with same destination but newer timestamp
    AnnounceData announce2(dest, app_data2, new_time);
    addAnnounceData(announce2);

    // Should still be only 1 announce (de-duplicated)
    announces = getAnnounceData();
    TEST_ASSERT_EQUAL_MESSAGE(1, announces->size(),
        "Should have only 1 announce after de-duplication");

    // Should have the newer timestamp
    TEST_ASSERT_EQUAL_MESSAGE(new_time, announces->begin()->last_heard,
        "Should keep the newer announce");
}

// Test: Multiple unique destinations stored separately
void test_multiple_unique_destinations(void) {
    RNS::Bytes dest1 = makeDest(0xAA);
    RNS::Bytes dest2 = makeDest(0xBB);
    RNS::Bytes dest3 = makeDest(0xCC);
    RNS::Bytes app_data = makeAppData("Node");

    AnnounceData a1(dest1, app_data, 1000);
    AnnounceData a2(dest2, app_data, 2000);
    AnnounceData a3(dest3, app_data, 3000);

    addAnnounceData(a1);
    addAnnounceData(a2);
    addAnnounceData(a3);

    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL_MESSAGE(3, announces->size(),
        "Should have 3 unique announces");
}

// Test: Sorted set is ordered by timestamp (ascending)
void test_announces_sorted_by_timestamp(void) {
    RNS::Bytes dest1 = makeDest(0x11);
    RNS::Bytes dest2 = makeDest(0x22);
    RNS::Bytes dest3 = makeDest(0x33);
    RNS::Bytes app_data = makeAppData("Node");

    // Add out of order
    AnnounceData a2(dest2, app_data, 2000);
    AnnounceData a1(dest1, app_data, 1000);
    AnnounceData a3(dest3, app_data, 3000);

    addAnnounceData(a2);
    addAnnounceData(a1);
    addAnnounceData(a3);

    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL(3, announces->size());

    // Verify sorted order (ascending by timestamp based on operator<)
    auto it = announces->begin();
    TEST_ASSERT_EQUAL(1000, it->last_heard);
    ++it;
    TEST_ASSERT_EQUAL(2000, it->last_heard);
    ++it;
    TEST_ASSERT_EQUAL(3000, it->last_heard);
}

// Test: De-dup then add new destination
void test_dedup_then_add_new(void) {
    RNS::Bytes dest1 = makeDest(0x11);
    RNS::Bytes dest2 = makeDest(0x22);
    RNS::Bytes app_data = makeAppData("Node");

    // Add dest1 twice (should de-dup)
    AnnounceData a1_old(dest1, app_data, 1000);
    AnnounceData a1_new(dest1, app_data, 2000);
    addAnnounceData(a1_old);
    addAnnounceData(a1_new);

    // Add dest2
    AnnounceData a2(dest2, app_data, 3000);
    addAnnounceData(a2);

    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL_MESSAGE(2, announces->size(),
        "Should have 2 announces: de-duped dest1 + dest2");

    // Verify timestamps
    auto it = announces->begin();
    TEST_ASSERT_EQUAL(2000, it->last_heard);  // dest1 updated
    ++it;
    TEST_ASSERT_EQUAL(3000, it->last_heard);  // dest2
}

// Test: Announces persist across simulated reboot
void test_announces_persist_across_reboot(void) {
    RNS::Bytes dest1 = makeDest(0x11);
    RNS::Bytes dest2 = makeDest(0x22);
    RNS::Bytes app_data = makeAppData("Node");

    AnnounceData a1(dest1, app_data, 1000);
    AnnounceData a2(dest2, app_data, 2000);

    addAnnounceData(a1);
    addAnnounceData(a2);

    // Persist to disk
    persistAnnounceData();

    // Simulate reboot
    resetAllState();

    // Reload
    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL_MESSAGE(2, announces->size(),
        "Announces should persist across reboot");

    // Verify correct data loaded
    auto it = announces->begin();
    TEST_ASSERT_EQUAL(1000, it->last_heard);
    ++it;
    TEST_ASSERT_EQUAL(2000, it->last_heard);
}

// Test: De-duplication persists correctly
void test_dedup_persists_correctly(void) {
    RNS::Bytes dest = makeDest(0x55);
    RNS::Bytes app_data = makeAppData("Node");

    // Add twice with different timestamps
    AnnounceData a1(dest, app_data, 1000);
    addAnnounceData(a1);

    AnnounceData a2(dest, app_data, 2000);
    addAnnounceData(a2);

    // Persist
    persistAnnounceData();

    // Simulate reboot
    resetAllState();

    // Reload
    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL_MESSAGE(1, announces->size(),
        "De-duplicated announce should persist as single entry");
    TEST_ASSERT_EQUAL_MESSAGE(2000, announces->begin()->last_heard,
        "Should persist the newer timestamp");
}

// Test: Rapid updates to same destination
void test_rapid_updates_same_destination(void) {
    RNS::Bytes dest = makeDest(0x77);
    RNS::Bytes app_data = makeAppData("Node");

    // Simulate rapid announce updates (like hearing same node multiple times)
    for (int i = 0; i < 10; i++) {
        AnnounceData a(dest, app_data, 1000 + i * 100);
        addAnnounceData(a);
    }

    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL_MESSAGE(1, announces->size(),
        "Rapid updates should result in single entry");
    TEST_ASSERT_EQUAL_MESSAGE(1900, announces->begin()->last_heard,
        "Should have the most recent timestamp");
}

// Test: Mixed de-dup and unique adds
void test_mixed_dedup_and_unique(void) {
    RNS::Bytes app_data = makeAppData("Node");

    // Add 5 unique destinations
    for (int i = 0; i < 5; i++) {
        RNS::Bytes dest = makeDest(0x10 + i);
        AnnounceData a(dest, app_data, 1000 + i * 100);
        addAnnounceData(a);
    }

    TEST_ASSERT_EQUAL(5, getAnnounceData()->size());

    // Update 3 of them (de-dup)
    for (int i = 0; i < 3; i++) {
        RNS::Bytes dest = makeDest(0x10 + i);
        AnnounceData a(dest, app_data, 5000 + i * 100);
        addAnnounceData(a);
    }

    // Should still have 5 unique destinations
    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL_MESSAGE(5, announces->size(),
        "Should still have 5 unique destinations after updates");
}

// Test: Empty state returns empty set
void test_empty_state(void) {
    const set<AnnounceData>* announces = getAnnounceData();
    TEST_ASSERT_EQUAL(0, announces->size());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_add_single_announce);
    RUN_TEST(test_dedup_same_destination_replaces_older);
    RUN_TEST(test_multiple_unique_destinations);
    RUN_TEST(test_announces_sorted_by_timestamp);
    RUN_TEST(test_dedup_then_add_new);
    RUN_TEST(test_announces_persist_across_reboot);
    RUN_TEST(test_dedup_persists_correctly);
    RUN_TEST(test_rapid_updates_same_destination);
    RUN_TEST(test_mixed_dedup_and_unique);
    RUN_TEST(test_empty_state);

    return UNITY_END();
}
