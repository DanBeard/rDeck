/**
 * Unit tests for RNS::Bytes class
 */

#include <unity.h>
#include "Bytes.h"

using namespace RNS;

void setUp(void) {}
void tearDown(void) {}

void test_default_constructor_creates_empty(void) {
    Bytes b;
    TEST_ASSERT_EQUAL(0, b.size());
    TEST_ASSERT_TRUE(b.empty());
    TEST_ASSERT_FALSE(b);
}

void test_none_constructor(void) {
    Bytes b(Bytes::NONE);
    TEST_ASSERT_EQUAL(0, b.size());
    TEST_ASSERT_FALSE(b);
}

void test_construct_from_chunk(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    Bytes b(data, sizeof(data));
    TEST_ASSERT_EQUAL(4, b.size());
    TEST_ASSERT_EQUAL(0x01, b.data()[0]);
    TEST_ASSERT_EQUAL(0x04, b.data()[3]);
    TEST_ASSERT_TRUE(b);
}

void test_construct_from_string(void) {
    Bytes b("hello");
    TEST_ASSERT_EQUAL(5, b.size());
    TEST_ASSERT_EQUAL_STRING("hello", b.toString().c_str());
}

void test_copy_constructor(void) {
    Bytes a("test");
    Bytes b(a);
    TEST_ASSERT_EQUAL(4, b.size());
    TEST_ASSERT_EQUAL_STRING("test", b.toString().c_str());
}

void test_assignment_operator(void) {
    Bytes a("foo");
    Bytes b;
    b = a;
    TEST_ASSERT_EQUAL(3, b.size());
    TEST_ASSERT_EQUAL_STRING("foo", b.toString().c_str());
}

void test_append_bytes(void) {
    Bytes a("hello");
    Bytes b(" world");
    a.append(b);
    TEST_ASSERT_EQUAL(11, a.size());
    TEST_ASSERT_EQUAL_STRING("hello world", a.toString().c_str());
}

void test_append_single_byte(void) {
    Bytes a("ab");
    a.append((uint8_t)'c');
    TEST_ASSERT_EQUAL(3, a.size());
    TEST_ASSERT_EQUAL_STRING("abc", a.toString().c_str());
}

void test_concatenation_operator(void) {
    Bytes a("foo");
    Bytes b("bar");
    Bytes c = a + b;
    TEST_ASSERT_EQUAL(6, c.size());
    TEST_ASSERT_EQUAL_STRING("foobar", c.toString().c_str());
    // originals unchanged
    TEST_ASSERT_EQUAL(3, a.size());
    TEST_ASSERT_EQUAL(3, b.size());
}

void test_plus_equals_operator(void) {
    Bytes a("foo");
    a += Bytes("bar");
    TEST_ASSERT_EQUAL_STRING("foobar", a.toString().c_str());
}

void test_equality_operator(void) {
    Bytes a("test");
    Bytes b("test");
    Bytes c("other");
    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_FALSE(a == c);
    TEST_ASSERT_TRUE(a != c);
}

void test_left(void) {
    Bytes b("abcdef");
    Bytes l = b.left(3);
    TEST_ASSERT_EQUAL(3, l.size());
    TEST_ASSERT_EQUAL_STRING("abc", l.toString().c_str());
}

void test_right(void) {
    Bytes b("abcdef");
    Bytes r = b.right(3);
    TEST_ASSERT_EQUAL(3, r.size());
    TEST_ASSERT_EQUAL_STRING("def", r.toString().c_str());
}

void test_mid(void) {
    Bytes b("abcdef");
    Bytes m = b.mid(2, 3);
    TEST_ASSERT_EQUAL(3, m.size());
    TEST_ASSERT_EQUAL_STRING("cde", m.toString().c_str());
}

void test_mid_to_end(void) {
    Bytes b("abcdef");
    Bytes m = b.mid(4);
    TEST_ASSERT_EQUAL(2, m.size());
    TEST_ASSERT_EQUAL_STRING("ef", m.toString().c_str());
}

void test_hex_conversion(void) {
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    Bytes b(data, 4);
    std::string hex = b.toHex();
    TEST_ASSERT_EQUAL_STRING("deadbeef", hex.c_str());
}

void test_assign_hex(void) {
    Bytes b;
    b.assignHex("cafebabe");
    TEST_ASSERT_EQUAL(4, b.size());
    TEST_ASSERT_EQUAL(0xCA, b.data()[0]);
    TEST_ASSERT_EQUAL(0xFE, b.data()[1]);
    TEST_ASSERT_EQUAL(0xBA, b.data()[2]);
    TEST_ASSERT_EQUAL(0xBE, b.data()[3]);
}

void test_resize_grow(void) {
    Bytes b("abc");
    b.resize(6);
    TEST_ASSERT_EQUAL(6, b.size());
    TEST_ASSERT_EQUAL('a', b.data()[0]);
}

void test_resize_shrink(void) {
    Bytes b("abcdef");
    b.resize(3);
    TEST_ASSERT_EQUAL(3, b.size());
    TEST_ASSERT_EQUAL_STRING("abc", b.toString().c_str());
}

void test_clear(void) {
    Bytes b("data");
    b.clear();
    TEST_ASSERT_EQUAL(0, b.size());
    TEST_ASSERT_TRUE(b.empty());
    TEST_ASSERT_FALSE(b);
}

void test_find(void) {
    Bytes b("hello world");
    TEST_ASSERT_EQUAL(6, b.find("world"));
    TEST_ASSERT_EQUAL(0, b.find("hello"));
    TEST_ASSERT_EQUAL(-1, b.find("xyz"));
}

void test_subscript_operator(void) {
    uint8_t data[] = {10, 20, 30};
    Bytes b(data, 3);
    TEST_ASSERT_EQUAL(10, b[0]);
    TEST_ASSERT_EQUAL(20, b[1]);
    TEST_ASSERT_EQUAL(30, b[2]);
}

void test_stream_append_operator(void) {
    Bytes a("foo");
    Bytes b("bar");
    a << b;
    TEST_ASSERT_EQUAL_STRING("foobar", a.toString().c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_default_constructor_creates_empty);
    RUN_TEST(test_none_constructor);
    RUN_TEST(test_construct_from_chunk);
    RUN_TEST(test_construct_from_string);
    RUN_TEST(test_copy_constructor);
    RUN_TEST(test_assignment_operator);
    RUN_TEST(test_append_bytes);
    RUN_TEST(test_append_single_byte);
    RUN_TEST(test_concatenation_operator);
    RUN_TEST(test_plus_equals_operator);
    RUN_TEST(test_equality_operator);
    RUN_TEST(test_left);
    RUN_TEST(test_right);
    RUN_TEST(test_mid);
    RUN_TEST(test_mid_to_end);
    RUN_TEST(test_hex_conversion);
    RUN_TEST(test_assign_hex);
    RUN_TEST(test_resize_grow);
    RUN_TEST(test_resize_shrink);
    RUN_TEST(test_clear);
    RUN_TEST(test_find);
    RUN_TEST(test_subscript_operator);
    RUN_TEST(test_stream_append_operator);

    return UNITY_END();
}
