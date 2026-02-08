/**
 * Unit tests for LXMF conversation persistence
 *
 * Tests that conversations are properly persisted to disk and can be
 * recovered after a simulated reboot (clearing in-memory state).
 *
 * These tests verify the fix for the bug where conversations would
 * disappear from the Msgs tab after reboot because persistAllConversationInfo()
 * was never being called.
 */

#include <unity.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

#include "FS.h"  // Include full FS definition for delete
#include "services/RnsUtils/LXMFData.h"

using namespace Retcon::LXMF;

// Test fixture state
static FS* test_fs = nullptr;
static char test_dir[256] = {0};

// Helper to create unique temp directory for each test
static void create_test_dir() {
    snprintf(test_dir, sizeof(test_dir), "/tmp/rdeck_test_%d_%ld",
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

// Helper to create test hash
static RNS::Bytes makeHash(uint8_t fill) {
    uint8_t data[16];
    memset(data, fill, 16);
    return RNS::Bytes(data, 16);
}

void setUp(void) {
    // Create fresh temp directory for this test
    create_test_dir();

    // Create filesystem pointing to temp directory
    test_fs = new FS(test_dir);

    // Set the test filesystem for LXMFData to use
    setTestFilesystem(test_fs);

    // Reset LXMF state for clean test
    resetAllState();
}

void tearDown(void) {
    // Reset state before cleanup
    resetAllState();

    // Clear the test filesystem
    setTestFilesystem(nullptr);

    // Clean up filesystem
    delete test_fs;
    test_fs = nullptr;

    // Remove test directory
    remove_test_dir();
}

// Test: addMessageToConversation persists the conversation list
void test_add_message_persists_conversation_list(void) {
    RNS::Bytes their_hash = makeHash(0x11);
    RNS::Bytes my_hash = makeHash(0x22);

    // Create and add a message
    Message msg(my_hash, their_hash, "Hello", "World");
    msg.timestamp = time(nullptr);
    msg.status = Message::STATUS::SENT;

    addMessageToConversation(msg, their_hash);

    // Verify conversation is in memory
    set<ConversationMetaInfo>* convs = getAllConversationInfo();
    TEST_ASSERT_EQUAL(1, convs->size());

    // Simulate reboot by resetting state
    resetAllState();

    // Reload from disk - should find the persisted conversation
    set<ConversationMetaInfo>* convs_after_reboot = getAllConversationInfo();
    TEST_ASSERT_EQUAL_MESSAGE(1, convs_after_reboot->size(),
        "Conversation should persist across simulated reboot");

    // Verify it's the correct conversation
    auto it = convs_after_reboot->begin();
    TEST_ASSERT_EQUAL(16, it->their_hash.size());
    TEST_ASSERT_EQUAL(0x11, it->their_hash.data()[0]);
}

// Test: loadAsCurrentConversation persists the conversation list
void test_load_conversation_persists_list(void) {
    RNS::Bytes their_hash = makeHash(0x33);

    // Load a conversation (simulates clicking on announce)
    Conversation* conv = loadAsCurrentConversation(their_hash);
    TEST_ASSERT_NOT_NULL(conv);
    TEST_ASSERT_EQUAL(16, conv->info.their_hash.size());

    // Verify it's in the in-memory list
    set<ConversationMetaInfo>* convs = getAllConversationInfo();
    TEST_ASSERT_EQUAL(1, convs->size());

    // Simulate reboot
    resetAllState();

    // Reload - should find the persisted conversation
    set<ConversationMetaInfo>* convs_after_reboot = getAllConversationInfo();
    TEST_ASSERT_EQUAL_MESSAGE(1, convs_after_reboot->size(),
        "Conversation loaded via loadAsCurrentConversation should persist");

    auto it = convs_after_reboot->begin();
    TEST_ASSERT_EQUAL(0x33, it->their_hash.data()[0]);
}

// Test: Multiple conversations persist correctly
void test_multiple_conversations_persist(void) {
    RNS::Bytes hash1 = makeHash(0xAA);
    RNS::Bytes hash2 = makeHash(0xBB);
    RNS::Bytes hash3 = makeHash(0xCC);
    RNS::Bytes my_hash = makeHash(0x00);

    // Add messages to different conversations
    Message msg1(my_hash, hash1, "", "Message 1");
    msg1.timestamp = 1000;
    msg1.status = Message::STATUS::SENT;
    addMessageToConversation(msg1, hash1);

    Message msg2(my_hash, hash2, "", "Message 2");
    msg2.timestamp = 2000;
    msg2.status = Message::STATUS::SENT;
    addMessageToConversation(msg2, hash2);

    Message msg3(my_hash, hash3, "", "Message 3");
    msg3.timestamp = 3000;
    msg3.status = Message::STATUS::SENT;
    addMessageToConversation(msg3, hash3);

    // Verify all in memory
    set<ConversationMetaInfo>* convs = getAllConversationInfo();
    TEST_ASSERT_EQUAL(3, convs->size());

    // Simulate reboot
    resetAllState();

    // Reload and verify all 3 conversations persist
    set<ConversationMetaInfo>* convs_after = getAllConversationInfo();
    TEST_ASSERT_EQUAL_MESSAGE(3, convs_after->size(),
        "All 3 conversations should persist across reboot");
}

// Test: Messages within a conversation persist correctly
void test_messages_persist_within_conversation(void) {
    RNS::Bytes their_hash = makeHash(0x55);
    RNS::Bytes my_hash = makeHash(0x66);

    // Add multiple messages to same conversation
    for (int i = 0; i < 5; i++) {
        Message msg(my_hash, their_hash, "", "Message " + std::to_string(i));
        msg.timestamp = 1000 + i;
        msg.status = Message::STATUS::SENT;
        addMessageToConversation(msg, their_hash);
    }

    // Load the conversation and verify messages
    Conversation* conv = loadAsCurrentConversation(their_hash);
    TEST_ASSERT_NOT_NULL(conv);
    TEST_ASSERT_EQUAL(5, conv->getMessages().size());

    // Simulate reboot
    resetAllState();

    // Reload conversation and verify messages persist
    Conversation* conv_after = loadAsCurrentConversation(their_hash);
    TEST_ASSERT_NOT_NULL(conv_after);
    TEST_ASSERT_EQUAL_MESSAGE(5, conv_after->getMessages().size(),
        "All 5 messages should persist within the conversation");

    // Verify message content
    auto it = conv_after->getMessages().begin();
    TEST_ASSERT_EQUAL_STRING("Message 0", it->content.c_str());
}

// Test: Empty conversation list after fresh start
void test_empty_conversation_list_fresh_start(void) {
    // Don't add any conversations
    set<ConversationMetaInfo>* convs = getAllConversationInfo();
    TEST_ASSERT_EQUAL(0, convs->size());

    // Simulate reboot
    resetAllState();

    // Should still be empty
    set<ConversationMetaInfo>* convs_after = getAllConversationInfo();
    TEST_ASSERT_EQUAL(0, convs_after->size());
}

// Test: Updating existing conversation updates persistence
void test_update_existing_conversation(void) {
    RNS::Bytes their_hash = makeHash(0x99);
    RNS::Bytes my_hash = makeHash(0x00);

    // Add first message
    Message msg1(my_hash, their_hash, "", "First");
    msg1.timestamp = 1000;
    msg1.status = Message::STATUS::SENT;
    addMessageToConversation(msg1, their_hash);

    // Verify
    set<ConversationMetaInfo>* convs = getAllConversationInfo();
    TEST_ASSERT_EQUAL(1, convs->size());
    TEST_ASSERT_EQUAL(1000, convs->begin()->last_message_at);

    // Add second message (updates existing conversation)
    Message msg2(my_hash, their_hash, "", "Second");
    msg2.timestamp = 2000;
    msg2.status = Message::STATUS::SENT;
    addMessageToConversation(msg2, their_hash);

    // Simulate reboot
    resetAllState();

    // Verify updated timestamp persists
    set<ConversationMetaInfo>* convs_after = getAllConversationInfo();
    TEST_ASSERT_EQUAL(1, convs_after->size());
    TEST_ASSERT_EQUAL_MESSAGE(2000, convs_after->begin()->last_message_at,
        "Updated timestamp should persist");

    // Verify both messages are in conversation
    Conversation* conv = loadAsCurrentConversation(their_hash);
    TEST_ASSERT_EQUAL(2, conv->getMessages().size());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_add_message_persists_conversation_list);
    RUN_TEST(test_load_conversation_persists_list);
    RUN_TEST(test_multiple_conversations_persist);
    RUN_TEST(test_messages_persist_within_conversation);
    RUN_TEST(test_empty_conversation_list_fresh_start);
    RUN_TEST(test_update_existing_conversation);

    return UNITY_END();
}
