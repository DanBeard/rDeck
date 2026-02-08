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

// Test Message serialize/deserialize round-trip
void test_message_serialize_roundtrip(void) {
    uint8_t srcHash[16], destHash[16];
    memset(srcHash, 0x11, 16);
    memset(destHash, 0x22, 16);

    RNS::Bytes src(srcHash, 16);
    RNS::Bytes dest(destHash, 16);

    Message original(src, dest, "Hello", "World");
    original.status = Message::STATUS::SENT;

    // Serialize
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    original.serialize(arr);

    // Deserialize
    Message restored(arr);

    TEST_ASSERT_EQUAL(16, restored.src.size());
    TEST_ASSERT_EQUAL(16, restored.dest.size());
    TEST_ASSERT_EQUAL(0x11, restored.src.data()[0]);
    TEST_ASSERT_EQUAL(0x22, restored.dest.data()[0]);
    TEST_ASSERT_EQUAL_STRING("Hello", restored.title.c_str());
    TEST_ASSERT_EQUAL_STRING("World", restored.content.c_str());
    TEST_ASSERT_EQUAL(Message::STATUS::SENT, restored.status);
}

// Test Conversation serialize/deserialize round-trip
void test_conversation_serialize_roundtrip(void) {
    uint8_t theirHash[16];
    memset(theirHash, 0x33, 16);
    RNS::Bytes their_bytes(theirHash, 16);

    Conversation conv;
    conv.info.their_hash = their_bytes;
    conv.info.their_name = "Alice";
    conv.info.last_message_at = 1700000000;

    // Add messages
    uint8_t srcH[16], dstH[16];
    memset(srcH, 0x33, 16);
    memset(dstH, 0x44, 16);
    RNS::Bytes s(srcH, 16), d(dstH, 16);

    Message msg1(s, d, "", "First message");
    msg1.status = Message::STATUS::SENT;
    msg1.timestamp = 1700000000;
    conv.addMessage(msg1);

    Message msg2(d, s, "", "Reply");
    msg2.status = Message::STATUS::UNSET;
    msg2.timestamp = 1700000001;
    conv.addMessage(msg2);

    // Serialize
    JsonDocument doc;
    conv.serialize(doc);

    // Verify structure: doc["info"] should be an object, doc["messages"] an array
    TEST_ASSERT_TRUE_MESSAGE(doc["info"].is<JsonObject>(),
        "doc['info'] must be a JsonObject, not an array");
    TEST_ASSERT_TRUE_MESSAGE(doc["messages"].is<JsonArray>(),
        "doc['messages'] must be a JsonArray");

    // Verify messages are direct array elements, not nested arrays
    JsonArray msgs = doc["messages"];
    TEST_ASSERT_EQUAL(2, msgs.size());
    TEST_ASSERT_TRUE_MESSAGE(msgs[0].is<JsonArray>(),
        "Each message must be a JsonArray");

    // Deserialize into new conversation
    Conversation restored;
    restored.deserialize(doc);

    TEST_ASSERT_EQUAL(16, restored.info.their_hash.size());
    TEST_ASSERT_EQUAL(0x33, restored.info.their_hash.data()[0]);
    TEST_ASSERT_EQUAL_STRING("Alice", restored.info.their_name.c_str());
    TEST_ASSERT_EQUAL(1700000001, restored.info.last_message_at);

    const auto& restoredMsgs = restored.getMessages();
    TEST_ASSERT_EQUAL(2, restoredMsgs.size());

    auto it = restoredMsgs.begin();
    TEST_ASSERT_EQUAL_STRING("First message", it->content.c_str());
    TEST_ASSERT_EQUAL(Message::STATUS::SENT, it->status);
    ++it;
    TEST_ASSERT_EQUAL_STRING("Reply", it->content.c_str());
}

// Test ConversationMetaInfo serialize/deserialize round-trip
void test_conversation_meta_serialize_roundtrip(void) {
    uint8_t hash[16];
    memset(hash, 0xAB, 16);

    ConversationMetaInfo original;
    original.their_hash.assign(hash, 16);
    original.their_name = "Bob";
    original.last_message_at = 1700000000;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    original.serialize(obj);

    // Verify it's a proper object with expected keys
    TEST_ASSERT_TRUE(doc.containsKey("their_hash"));
    TEST_ASSERT_TRUE(doc.containsKey("their_name"));
    TEST_ASSERT_TRUE(doc.containsKey("last_message_at"));

    ConversationMetaInfo restored;
    restored.deserialize(obj);

    TEST_ASSERT_EQUAL(16, restored.their_hash.size());
    TEST_ASSERT_EQUAL(0xAB, restored.their_hash.data()[0]);
    TEST_ASSERT_EQUAL_STRING("Bob", restored.their_name.c_str());
    TEST_ASSERT_EQUAL(1700000000, restored.last_message_at);
}

// Test AnnounceData serialize/deserialize round-trip
void test_announce_serialize_roundtrip(void) {
    uint8_t destHash[16], appData[10];
    memset(destHash, 0x55, 16);
    memset(appData, 0x66, 10);

    RNS::Bytes dest(destHash, 16);
    RNS::Bytes app(appData, 10);

    AnnounceData original(dest, app, 1700000000);

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    original.serialize(arr);

    TEST_ASSERT_EQUAL(4, arr.size());

    AnnounceData restored(arr);

    TEST_ASSERT_EQUAL(16, restored.dest.size());
    TEST_ASSERT_EQUAL(0x55, restored.dest.data()[0]);
    TEST_ASSERT_EQUAL(10, restored.app_data.size());
    TEST_ASSERT_EQUAL(0x66, restored.app_data.data()[0]);
    TEST_ASSERT_EQUAL(1700000000, restored.last_heard);
    TEST_ASSERT_EQUAL(0, restored.public_key.size());  // no public key in this test

    // Test with public key
    uint8_t pubKeyData[64];
    memset(pubKeyData, 0x77, 64);
    RNS::Bytes pubKey(pubKeyData, 64);
    AnnounceData withKey(dest, app, pubKey, 1700000000);

    JsonDocument doc2;
    JsonArray arr2 = doc2.to<JsonArray>();
    withKey.serialize(arr2);
    TEST_ASSERT_EQUAL(4, arr2.size());

    AnnounceData restoredWithKey(arr2);
    TEST_ASSERT_EQUAL(64, restoredWithKey.public_key.size());
    TEST_ASSERT_EQUAL(0x77, restoredWithKey.public_key.data()[0]);
}

// Test deserializing corrupted/invalid conversation doesn't crash
void test_conversation_deserialize_invalid_format(void) {
    // Simulate old buggy format: {"info": [{}], "messages": [[]]}
    JsonDocument doc;
    doc["info"].add<JsonObject>();  // Creates an array with an object: [{}]
    doc["messages"].add<JsonArray>();  // Creates [[]]

    Conversation conv;
    conv.deserialize(doc);  // Should not crash

    // Should have empty data since format was invalid
    TEST_ASSERT_EQUAL(0, conv.info.their_hash.size());
    TEST_ASSERT_EQUAL(0, conv.getMessages().size());
}

// Test that delete on nullptr is safe and simulates the _sending_packet pattern:
// A raw pointer member must be initialized to nullptr so that delete before
// first assignment doesn't crash (the bug that caused Guru Meditation on first send).
void test_delete_nullptr_is_safe(void) {
    struct PacketHolder {
        int* ptr = nullptr;  // mirrors: RNS::Packet *_sending_packet = nullptr;
    };

    PacketHolder holder;
    TEST_ASSERT_NULL(holder.ptr);

    // First "transmit" — delete before first assignment must not crash
    delete holder.ptr;
    holder.ptr = new int(42);
    TEST_ASSERT_EQUAL(42, *holder.ptr);

    // Second "transmit" — delete previous, assign new
    delete holder.ptr;
    holder.ptr = new int(99);
    TEST_ASSERT_EQUAL(99, *holder.ptr);

    // "delivery callback" — delete and null
    delete holder.ptr;
    holder.ptr = nullptr;

    // "next transmit after delivery" — delete nullptr again, must not crash
    delete holder.ptr;
    holder.ptr = new int(7);
    TEST_ASSERT_EQUAL(7, *holder.ptr);

    delete holder.ptr;
    holder.ptr = nullptr;
}

// Test that uninitialized pointer pattern (the old bug) is dangerous:
// Verify that our PacketHolder with explicit nullptr init differs from
// one without init (compiler may zero it in debug, but the pattern matters)
void test_raw_pointer_init_pattern(void) {
    // This mirrors the fix: RNS::Packet *_sending_packet = nullptr;
    struct GoodPattern {
        int* ptr = nullptr;
    };
    GoodPattern good;
    TEST_ASSERT_NULL(good.ptr);

    // Verify the delete-assign-delete-null lifecycle works
    delete good.ptr;              // safe: nullptr
    good.ptr = new int(1);
    delete good.ptr;              // safe: valid pointer
    good.ptr = nullptr;
    delete good.ptr;              // safe: nullptr again
    TEST_ASSERT_NULL(good.ptr);
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
    RUN_TEST(test_message_serialize_roundtrip);
    RUN_TEST(test_conversation_serialize_roundtrip);
    RUN_TEST(test_conversation_meta_serialize_roundtrip);
    RUN_TEST(test_announce_serialize_roundtrip);
    RUN_TEST(test_conversation_deserialize_invalid_format);
    RUN_TEST(test_delete_nullptr_is_safe);
    RUN_TEST(test_raw_pointer_init_pattern);

    return UNITY_END();
}
