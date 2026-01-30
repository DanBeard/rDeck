/**
 * Unit tests for LXMF Message class
 *
 * Tests the basic Message construction and data handling
 * without requiring the full RetOS system.
 */

#include <unity.h>
#include "services/RnsUtils/LXMFData.h"

using namespace Retcon::LXMF;

void setUp(void) {
    // No global state needed for these tests
}

void tearDown(void) {
    // Nothing to clean up
}

// Test default Message constructor creates empty message
void test_message_default_constructor(void) {
    Message msg;

    // Default message should have empty fields
    TEST_ASSERT_EQUAL(0, msg.src.size());
    TEST_ASSERT_EQUAL(0, msg.dest.size());
    TEST_ASSERT_EQUAL(0, msg.signature.size());
    TEST_ASSERT_EQUAL(0, msg.packed_payload.size());
    TEST_ASSERT_TRUE(msg.title.empty());
    TEST_ASSERT_TRUE(msg.content.empty());
    TEST_ASSERT_EQUAL(0, msg.timestamp);
    TEST_ASSERT_EQUAL(Message::STATUS::UNSET, msg.status);
    TEST_ASSERT_EQUAL(Message::SENDER::UNKNOWN, msg.sender);
}

// Test Message construction from raw bytes (wire format)
void test_message_from_bytes_extracts_fields(void) {
    // Build a mock wire message:
    // [dest: 16 bytes][src: 16 bytes][signature: 64 bytes][payload: remainder]
    uint8_t rawMsg[16 + 16 + 64 + 10]; // 106 bytes total
    memset(rawMsg, 0, sizeof(rawMsg));

    // Fill dest with 0xAA
    memset(rawMsg, 0xAA, 16);
    // Fill src with 0xBB
    memset(rawMsg + 16, 0xBB, 16);
    // Fill signature with 0xCC
    memset(rawMsg + 32, 0xCC, 64);
    // Fill payload with 0xDD
    memset(rawMsg + 96, 0xDD, 10);

    RNS::Bytes wireBytes(rawMsg, sizeof(rawMsg));
    Message msg(wireBytes);

    // Verify dest extraction (first 16 bytes)
    TEST_ASSERT_EQUAL(16, msg.dest.size());
    TEST_ASSERT_EQUAL(0xAA, msg.dest.data()[0]);
    TEST_ASSERT_EQUAL(0xAA, msg.dest.data()[15]);

    // Verify src extraction (bytes 16-31)
    TEST_ASSERT_EQUAL(16, msg.src.size());
    TEST_ASSERT_EQUAL(0xBB, msg.src.data()[0]);
    TEST_ASSERT_EQUAL(0xBB, msg.src.data()[15]);

    // Verify signature extraction (bytes 32-95)
    TEST_ASSERT_EQUAL(64, msg.signature.size());
    TEST_ASSERT_EQUAL(0xCC, msg.signature.data()[0]);
    TEST_ASSERT_EQUAL(0xCC, msg.signature.data()[63]);

    // Verify payload extraction (bytes 96+)
    TEST_ASSERT_EQUAL(10, msg.packed_payload.size());
    TEST_ASSERT_EQUAL(0xDD, msg.packed_payload.data()[0]);
    TEST_ASSERT_EQUAL(0xDD, msg.packed_payload.data()[9]);
}

// Test Message construction from components
void test_message_from_components(void) {
    uint8_t srcHash[16], destHash[16];
    memset(srcHash, 0x11, 16);
    memset(destHash, 0x22, 16);

    RNS::Bytes src(srcHash, 16);
    RNS::Bytes dest(destHash, 16);

    Message msg(src, dest, "Test Title", "Test Content");

    TEST_ASSERT_EQUAL(16, msg.src.size());
    TEST_ASSERT_EQUAL(16, msg.dest.size());
    TEST_ASSERT_EQUAL_STRING("Test Title", msg.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Test Content", msg.content.c_str());

    // Status should still be default
    TEST_ASSERT_EQUAL(Message::STATUS::UNSET, msg.status);
}

// Test fullMsg() returns correct concatenation
void test_message_full_msg_concatenation(void) {
    uint8_t rawMsg[16 + 16 + 64 + 5]; // dest + src + sig + payload
    memset(rawMsg, 0, sizeof(rawMsg));

    memset(rawMsg, 0xAA, 16);           // dest
    memset(rawMsg + 16, 0xBB, 16);      // src
    memset(rawMsg + 32, 0xCC, 64);      // signature
    memset(rawMsg + 96, 0xDD, 5);       // payload

    RNS::Bytes wireBytes(rawMsg, sizeof(rawMsg));
    Message msg(wireBytes);

    RNS::Bytes full = msg.fullMsg();

    // fullMsg() returns: dest + src + signature + packed_payload
    TEST_ASSERT_EQUAL(16 + 16 + 64 + 5, full.size());

    // Verify the concatenation order
    TEST_ASSERT_EQUAL(0xAA, full.data()[0]);      // dest start
    TEST_ASSERT_EQUAL(0xBB, full.data()[16]);     // src start
    TEST_ASSERT_EQUAL(0xCC, full.data()[32]);     // signature start
    TEST_ASSERT_EQUAL(0xDD, full.data()[96]);     // payload start
}

// Test status enum values are distinct
void test_message_status_enum_values(void) {
    // Ensure status values are distinct and ordered as expected
    TEST_ASSERT_EQUAL(0, Message::STATUS::UNSET);
    TEST_ASSERT_NOT_EQUAL(Message::STATUS::UNSET, Message::STATUS::QUEUEING);
    TEST_ASSERT_NOT_EQUAL(Message::STATUS::QUEUEING, Message::STATUS::SENDING);
    TEST_ASSERT_NOT_EQUAL(Message::STATUS::SENDING, Message::STATUS::RETRY);

    // COMPLETE_START should be a boundary marker
    TEST_ASSERT_TRUE(Message::STATUS::SENT >= Message::STATUS::COMPLETE_START);
    TEST_ASSERT_TRUE(Message::STATUS::FAILED >= Message::STATUS::COMPLETE_START);
}

// Test sender detection helper
void test_message_sent_by_them_detection(void) {
    uint8_t theirHash[16], myHash[16];
    memset(theirHash, 0x11, 16);
    memset(myHash, 0x22, 16);

    RNS::Bytes theirBytes(theirHash, 16);
    RNS::Bytes myBytes(myHash, 16);

    // Create message where src matches their hash
    Message msgFromThem(theirBytes, myBytes, "Title", "Content");
    TEST_ASSERT_TRUE(msgFromThem.msgSentByThem(theirBytes));
    TEST_ASSERT_EQUAL(Message::SENDER::THEM, msgFromThem.sender);

    // Create message where src does NOT match their hash
    Message msgFromMe(myBytes, theirBytes, "Title", "Content");
    TEST_ASSERT_FALSE(msgFromMe.msgSentByThem(theirBytes));
    TEST_ASSERT_EQUAL(Message::SENDER::ME, msgFromMe.sender);
}

// Test max payload size constant
void test_message_max_payload_size(void) {
    // LXMF max payload is 435 bytes per spec
    TEST_ASSERT_EQUAL(435, Message::max_lxmf_payload_size);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_message_default_constructor);
    RUN_TEST(test_message_from_bytes_extracts_fields);
    RUN_TEST(test_message_from_components);
    RUN_TEST(test_message_full_msg_concatenation);
    RUN_TEST(test_message_status_enum_values);
    RUN_TEST(test_message_sent_by_them_detection);
    RUN_TEST(test_message_max_payload_size);

    return UNITY_END();
}
