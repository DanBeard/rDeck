#include <unity.h>
#include <ArduinoJson.h>

void setUp(void) {}
void tearDown(void) {}

void test_announce_msgpack_format(void) {
    uint8_t buffer[200];
    JsonDocument doc;
    
    const char* name = "TestDevice";
    doc.add(MsgPackBinary(name, strlen(name)));
    doc.add(nullptr);
    
    size_t bytesWritten = serializeMsgPack(doc, buffer, 200);
    
    // Print actual bytes for debugging
    printf("Announce bytes (%zu): ", bytesWritten);
    for (size_t i = 0; i < bytesWritten; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");
    
    // Expected format (same as Python LXMF):
    // 92 = fixarray(2)
    // c4 0a = bin8 with length 10
    // 54 65 73 74 44 65 76 69 63 65 = "TestDevice"
    // c0 = nil
    
    TEST_ASSERT_EQUAL(14, bytesWritten);  // 1 + 2 + 10 + 1 = 14 bytes
    TEST_ASSERT_EQUAL_HEX8(0x92, buffer[0]);  // fixarray(2)
    TEST_ASSERT_EQUAL_HEX8(0xc4, buffer[1]);  // bin8
    TEST_ASSERT_EQUAL_HEX8(0x0a, buffer[2]);  // length 10
    TEST_ASSERT_EQUAL_HEX8(0xc0, buffer[13]); // nil at the end
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_announce_msgpack_format);
    return UNITY_END();
}
