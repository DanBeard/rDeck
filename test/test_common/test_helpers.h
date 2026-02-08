#pragma once

#include <unity.h>
#include <stdint.h>
#include <string.h>

// Helper to compare byte arrays
#define TEST_ASSERT_BYTES_EQUAL(expected, actual, len) \
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, len)

// Helper to create a byte array from hex string (compile-time known size)
// Usage: uint8_t bytes[16]; hexToBytes("0123456789ABCDEF0123456789ABCDEF", bytes, 16);
inline void hexToBytes(const char* hex, uint8_t* out, size_t outLen) {
    for (size_t i = 0; i < outLen && hex[i*2] && hex[i*2+1]; i++) {
        char byte[3] = {hex[i*2], hex[i*2+1], 0};
        out[i] = (uint8_t)strtol(byte, nullptr, 16);
    }
}

// Helper to check if byte array is all zeros
inline bool isAllZeros(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] != 0) return false;
    }
    return true;
}

// Standard test setup/teardown - can be overridden per test file
#ifndef CUSTOM_SETUP_TEARDOWN
void setUp(void) {
    // Default empty setup
}

void tearDown(void) {
    // Default empty teardown
}
#endif
