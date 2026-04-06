#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>

void setUp(void) {}
void tearDown(void) {}

// Simulates the bug: assigning a temporary const char* to ArduinoJson
// stores the pointer, not a copy. When the source is freed, the value
// becomes garbage.
void test_const_char_ptr_becomes_dangling(void) {
    JsonDocument userInfo;

    {
        // Simulate lv_textarea_get_text returning a buffer that gets freed
        char tempBuffer[32];
        strncpy(tempBuffer, "MyDevice", sizeof(tempBuffer));
        const char* rawPtr = tempBuffer;

        // BAD: ArduinoJson stores the pointer, not a copy
        userInfo["name"] = rawPtr;

        // While buffer is alive, it works fine
        TEST_ASSERT_EQUAL_STRING("MyDevice", userInfo["name"].as<const char*>());

        // Corrupt the buffer to simulate textarea destruction
        memset(tempBuffer, 0xFF, sizeof(tempBuffer));
    }

    // The stored pointer now points to garbage
    const char* retrieved = userInfo["name"].as<const char*>();
    // This WOULD fail with the old code - the name is corrupted
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, memcmp(retrieved, "MyDevice", 8),
        "const char* should be dangling after source buffer is overwritten");
}

// Demonstrates the fix: assigning via String copies the data
void test_string_copy_survives_source_destruction(void) {
    JsonDocument userInfo;

    {
        char tempBuffer[32];
        strncpy(tempBuffer, "MyDevice", sizeof(tempBuffer));

        // FIX: Copy into String first, ArduinoJson copies String contents
        String name = tempBuffer;
        userInfo["name"] = name;

        // Corrupt the original buffer
        memset(tempBuffer, 0xFF, sizeof(tempBuffer));
    }

    // The value should survive because ArduinoJson copied the String
    const char* retrieved = userInfo["name"].as<const char*>();
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_STRING("MyDevice", retrieved);
}

// Test that device name persists through JSON serialization round-trip
void test_device_name_json_round_trip(void) {
    JsonDocument original;

    {
        String name = "Test Device 123";
        original["name"] = name;
    }

    // Serialize to JSON (simulates saveUserInfo)
    char jsonBuffer[256];
    size_t len = serializeJson(original, jsonBuffer, sizeof(jsonBuffer));
    TEST_ASSERT_GREATER_THAN(0, len);

    // Deserialize (simulates loadUserInfo)
    JsonDocument loaded;
    DeserializationError err = deserializeJson(loaded, jsonBuffer);
    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING("Test Device 123", loaded["name"].as<const char*>());
}

// Test empty name is rejected (matches the strnlen check in Settings.cpp)
void test_empty_name_not_saved(void) {
    JsonDocument userInfo;
    userInfo["name"] = "Original";

    const char* newName = "";
    // Simulates the guard: if (newName.length() > 0)
    if (newName && strnlen(newName, 1) > 0) {
        String nameCopy = newName;
        userInfo["name"] = nameCopy;
    }

    TEST_ASSERT_EQUAL_STRING("Original", userInfo["name"].as<const char*>());
}

// Test name with special characters round-trips correctly
void test_special_characters_round_trip(void) {
    JsonDocument userInfo;

    String name = "Dan's rDeck #2";
    userInfo["name"] = name;

    char jsonBuffer[256];
    serializeJson(userInfo, jsonBuffer, sizeof(jsonBuffer));

    JsonDocument loaded;
    deserializeJson(loaded, jsonBuffer);
    TEST_ASSERT_EQUAL_STRING("Dan's rDeck #2", loaded["name"].as<const char*>());
}

// Test that name appears correctly in announce msgpack format
void test_device_name_in_announce_format(void) {
    JsonDocument userInfo;
    String name = "MyRDeck";
    userInfo["name"] = name;

    // Build announce payload the same way RnsService::announce() does
    JsonDocument doc;
    const char* announceName = userInfo["name"];
    TEST_ASSERT_NOT_NULL(announceName);

    doc.add(MsgPackBinary(announceName, strnlen(announceName, 100)));
    doc.add(nullptr);  // stamp_cost
    doc.add("rdeck");  // device_type

    uint8_t buffer[200];
    size_t bytesWritten = serializeMsgPack(doc, buffer, sizeof(buffer));

    // Should be: fixarray(3), bin8, len(7), "MyRDeck", nil, fixstr(5), "rdeck"
    TEST_ASSERT_EQUAL_HEX8(0x93, buffer[0]);  // fixarray(3)
    TEST_ASSERT_EQUAL_HEX8(0xc4, buffer[1]);  // bin8
    TEST_ASSERT_EQUAL_HEX8(7, buffer[2]);     // length 7

    // Verify the name bytes
    TEST_ASSERT_EQUAL_MEMORY("MyRDeck", &buffer[3], 7);
}

// Test overwriting a name properly replaces the old one
void test_overwrite_name(void) {
    JsonDocument userInfo;

    String name1 = "First Name";
    userInfo["name"] = name1;
    TEST_ASSERT_EQUAL_STRING("First Name", userInfo["name"].as<const char*>());

    String name2 = "Second Name";
    userInfo["name"] = name2;
    TEST_ASSERT_EQUAL_STRING("Second Name", userInfo["name"].as<const char*>());

    // Verify through serialization round-trip
    char jsonBuffer[256];
    serializeJson(userInfo, jsonBuffer, sizeof(jsonBuffer));

    JsonDocument loaded;
    deserializeJson(loaded, jsonBuffer);
    TEST_ASSERT_EQUAL_STRING("Second Name", loaded["name"].as<const char*>());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_const_char_ptr_becomes_dangling);
    RUN_TEST(test_string_copy_survives_source_destruction);
    RUN_TEST(test_device_name_json_round_trip);
    RUN_TEST(test_empty_name_not_saved);
    RUN_TEST(test_special_characters_round_trip);
    RUN_TEST(test_device_name_in_announce_format);
    RUN_TEST(test_overwrite_name);
    return UNITY_END();
}
