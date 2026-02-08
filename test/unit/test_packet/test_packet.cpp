/**
 * Unit tests for RNS::Packet class
 */

#include <unity.h>
#include "Packet.h"
#include "Identity.h"
#include "Destination.h"
#include "Transport.h"
#include "Bytes.h"
#include "Type.h"

using namespace RNS;

void setUp(void) {}
void tearDown(void) {}

// Packet construction with Destination and data
void test_packet_construction(void) {
    Identity id;
    Destination dest(id, Type::Destination::IN, Type::Destination::SINGLE, "test", "app");

    Bytes payload("hello packet");
    Packet pkt(dest, {Type::NONE}, payload);

    TEST_ASSERT_EQUAL(Type::Packet::DATA, pkt.packet_type());
    // destination_hash is populated after pack()
    pkt.pack();
    TEST_ASSERT_TRUE(pkt.destination_hash().size() > 0);
}

// Packet hash is computed and non-empty after packing
void test_packet_hash_computed(void) {
    Identity id;
    Destination dest(id, Type::Destination::IN, Type::Destination::SINGLE, "test", "hash");

    Bytes payload("hash test data");
    Packet pkt(dest, {Type::NONE}, payload);

    pkt.pack();
    TEST_ASSERT_TRUE(pkt.packet_hash().size() > 0);
}

// Packet raw data exists after packing
void test_packet_raw_after_pack(void) {
    Identity id;
    Destination dest(id, Type::Destination::IN, Type::Destination::SINGLE, "test", "raw");

    Bytes payload("raw test");
    Packet pkt(dest, {Type::NONE}, payload);

    pkt.pack();
    TEST_ASSERT_TRUE(pkt.raw().size() > 0);
}

// Header type defaults
void test_packet_header_type(void) {
    Identity id;
    Destination dest(id, Type::Destination::IN, Type::Destination::SINGLE, "test", "hdr");

    Bytes payload("header test");
    Packet pkt(dest, {Type::NONE}, payload, Type::Packet::DATA, Type::Packet::CONTEXT_NONE, Type::Transport::BROADCAST, Type::Packet::HEADER_1);

    TEST_ASSERT_EQUAL(Type::Packet::HEADER_1, pkt.header_type());
}

// PLAIN destination packet
void test_packet_plain_destination(void) {
    Destination dest({Type::NONE}, Type::Destination::IN, Type::Destination::PLAIN, "test", "plain");

    Bytes payload("plain data");
    Packet pkt(dest, {Type::NONE}, payload);

    pkt.pack();
    TEST_ASSERT_TRUE(pkt.raw().size() > 0);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_packet_construction);
    RUN_TEST(test_packet_hash_computed);
    RUN_TEST(test_packet_raw_after_pack);
    RUN_TEST(test_packet_header_type);
    RUN_TEST(test_packet_plain_destination);

    return UNITY_END();
}
