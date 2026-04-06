/**
 * Integration test for full LXMF send/receive workflow.
 *
 * Creates two identities (sender + receiver), sets up destinations,
 * packs LXMF messages into Reticulum packets, and injects the raw
 * bytes into a mock interface. Transport routes them to the registered
 * destination, which decrypts and delivers via callback.
 *
 * We avoid Packet::send() because Transport's duplicate packet filter
 * would reject the same hash on re-entry. Instead we pack() to get
 * raw bytes and inject directly via handle_incoming(), which is what
 * a real radio interface does when it receives data over the air.
 */

#include <unity.h>
#include <functional>
#include <string>
#include <vector>

#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Transport.h"
#include "Reticulum.h"
#include "Interface.h"
#include "FileSystem.h"
#include "Utilities/OS.h"
#include "Bytes.h"
#include "Type.h"
#include "services/RnsUtils/LXMFData.h"

using namespace RNS;
using namespace Retcon::LXMF;

// ---------------------------------------------------------------------------
// Stub FileSystem – satisfies Transport::start() without real storage
// ---------------------------------------------------------------------------
class StubFileSystemImpl : public FileSystemImpl {
public:
    bool file_exists(const char*) override { return false; }
    size_t read_file(const char*, Bytes&) override { return 0; }
    size_t write_file(const char*, const Bytes&) override { return 0; }
    FileStream open_file(const char*, FileStream::MODE) override { return {Type::NONE}; }
    bool remove_file(const char*) override { return false; }
    bool rename_file(const char*, const char*) override { return false; }
    bool directory_exists(const char*) override { return true; }
    bool create_directory(const char*) override { return true; }
    bool remove_directory(const char*) override { return false; }
    std::list<std::string> list_directory(const char*) override { return {}; }
    size_t storage_size() override { return 1024 * 1024; }
    size_t storage_available() override { return 1024 * 1024; }
};

// ---------------------------------------------------------------------------
// Mock Interface – injects raw bytes into Transport as incoming data
// ---------------------------------------------------------------------------
class MockInterface : public InterfaceImpl {
public:
    MockInterface() : InterfaceImpl("Mock") {
        _IN = true;
        _OUT = true;
        _online = true;
        _mode = Type::Interface::MODE_GATEWAY;
    }

    void send_outgoing(const Bytes&) override {
        // Outbound not used in these tests
    }

    // Simulate receiving raw bytes over the air
    void inject(const Bytes& data) {
        handle_incoming(data);
    }
};

// ---------------------------------------------------------------------------
// Global state for receive callback
// ---------------------------------------------------------------------------
static bool g_received = false;
static Bytes g_received_plaintext;

static void on_packet_received(const Bytes& data, const Packet& packet) {
    g_received = true;
    g_received_plaintext = data;
}

// Shared objects
static MockInterface* mock_impl = nullptr;
static Interface mock_iface({Type::NONE});
static Reticulum reticulum;

void setUp(void) {
    g_received = false;
    g_received_plaintext = Bytes();
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// One-time system initialization
// ---------------------------------------------------------------------------
static void init_system() {
    RNS::Utilities::OS::register_filesystem(*(new RNS::FileSystem(new StubFileSystemImpl())));

    mock_impl = new MockInterface();
    mock_iface = Interface(mock_impl);
    Transport::register_interface(mock_iface);

    reticulum = Reticulum();
    reticulum.transport_enabled(false);
    reticulum.start();
}

// Helper: pack a packet and inject raw bytes into Transport as incoming.
// This simulates another node transmitting and our interface receiving.
static void pack_and_inject(Packet& packet) {
    packet.pack();
    mock_impl->inject(packet.raw());
}

// ---------------------------------------------------------------------------
// Test 1: Full LXMF roundtrip – create, pack, encrypt, deliver, decrypt
// ---------------------------------------------------------------------------
void test_lxmf_full_roundtrip(void) {
    Identity sender_id;
    Destination sender_dest(sender_id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");

    Identity receiver_id;
    Destination receiver_in(receiver_id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");
    receiver_in.set_packet_callback(on_packet_received);
    receiver_in.set_proof_strategy(Type::Destination::PROVE_ALL);

    // OUT destination for addressing the receiver
    Destination receiver_out(receiver_id, Type::Destination::OUT, Type::Destination::SINGLE, "lxmf", "delivery");

    // Build LXMF message
    Message msg(sender_dest.hash(), receiver_in.hash(), "Hello", "World from LXMF test");
    msg.pack(sender_dest, receiver_out);

    TEST_ASSERT_TRUE(msg.packed_payload.size() > 0);
    TEST_ASSERT_EQUAL(64, msg.signature.size());

    // Create packet and inject
    Packet packet(receiver_out, msg.fullMsg());
    pack_and_inject(packet);

    // Verify reception
    TEST_ASSERT_TRUE(g_received);
    TEST_ASSERT_TRUE(g_received_plaintext.size() > 0);

    Message received_msg(g_received_plaintext);
    received_msg.unpack();

    TEST_ASSERT_EQUAL_STRING("Hello", received_msg.title.c_str());
    TEST_ASSERT_EQUAL_STRING("World from LXMF test", received_msg.content.c_str());
    TEST_ASSERT_TRUE(received_msg.dest == receiver_out.hash());
    TEST_ASSERT_TRUE(received_msg.src == sender_dest.hash());
}

// ---------------------------------------------------------------------------
// Test 2: Pack/unpack roundtrip preserves fields (no network)
// ---------------------------------------------------------------------------
void test_lxmf_pack_unpack_roundtrip(void) {
    Identity id_a;
    Destination dest_a(id_a, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");

    Identity id_b;
    Destination dest_b(id_b, Type::Destination::OUT, Type::Destination::SINGLE, "lxmf", "delivery");

    Message msg(dest_a.hash(), dest_b.hash(), "Test Title", "Test Body Content");
    msg.pack(dest_a, dest_b);

    TEST_ASSERT_TRUE(msg.packed_payload.size() > 0);
    TEST_ASSERT_EQUAL(64, msg.signature.size());
    TEST_ASSERT_TRUE(msg.timestamp > 0);

    Bytes wire = msg.fullMsg();
    Message reconstructed(wire);
    reconstructed.unpack();

    TEST_ASSERT_EQUAL_STRING("Test Title", reconstructed.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Test Body Content", reconstructed.content.c_str());
    TEST_ASSERT_EQUAL(msg.timestamp, reconstructed.timestamp);
}

// ---------------------------------------------------------------------------
// Test 3: Signature is present and valid
// ---------------------------------------------------------------------------
void test_lxmf_signature_valid(void) {
    Identity id;
    Destination src(id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");

    Identity id2;
    Destination dest(id2, Type::Destination::OUT, Type::Destination::SINGLE, "lxmf", "delivery");

    Message msg(src.hash(), dest.hash(), "Signed", "Message");
    msg.pack(src, dest);

    TEST_ASSERT_EQUAL(64, msg.signature.size());

    // Reconstruct signed material per LXMF spec
    Bytes hashed_part;
    hashed_part.append(dest.hash());
    hashed_part.append(src.hash());
    hashed_part.append(msg.packed_payload);
    Bytes hash = Identity::full_hash(hashed_part);
    Bytes signed_part;
    signed_part.append(hashed_part);
    signed_part.append(hash);

    TEST_ASSERT_TRUE(id.validate(msg.signature, signed_part));
}

// ---------------------------------------------------------------------------
// Test 4: Data is encrypted on the wire
// ---------------------------------------------------------------------------
void test_lxmf_encrypted_on_wire(void) {
    Identity receiver_id;
    Destination receiver_in(receiver_id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");
    receiver_in.set_packet_callback(on_packet_received);

    Destination receiver_out(receiver_id, Type::Destination::OUT, Type::Destination::SINGLE, "lxmf", "delivery");

    Identity sender_id;
    Destination sender_in(sender_id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");

    std::string long_content = "This is a longer message that tests the encryption pathway "
                               "through the Reticulum network stack. It contains enough data "
                               "to verify that encryption and decryption preserve all content.";
    Message msg(sender_in.hash(), receiver_in.hash(), "Encrypted Test", long_content);
    msg.pack(sender_in, receiver_out);

    // Pack into a Reticulum packet
    Packet packet(receiver_out, msg.fullMsg());
    packet.pack();

    // The raw bytes should NOT contain plaintext (encrypted for SINGLE dest)
    Bytes raw_bytes = packet.raw();
    TEST_ASSERT_EQUAL(-1, raw_bytes.find("Encrypted Test"));

    // Inject and verify decryption
    mock_impl->inject(packet.raw());
    TEST_ASSERT_TRUE(g_received);

    Message decrypted_msg(g_received_plaintext);
    decrypted_msg.unpack();
    TEST_ASSERT_EQUAL_STRING("Encrypted Test", decrypted_msg.title.c_str());
    TEST_ASSERT_EQUAL_STRING(long_content.c_str(), decrypted_msg.content.c_str());
}

// ---------------------------------------------------------------------------
// Test 5: Wrong destination doesn't receive the message
// ---------------------------------------------------------------------------
void test_lxmf_wrong_destination_no_receive(void) {
    Identity bystander_id;
    Destination bystander_dest(bystander_id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");
    bystander_dest.set_packet_callback(on_packet_received);

    Identity sender_id;
    Destination sender_dest(sender_id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");

    // Target has no IN destination registered (only OUT)
    Identity target_id;
    Destination target_out(target_id, Type::Destination::OUT, Type::Destination::SINGLE, "lxmf", "delivery");

    Message msg(sender_dest.hash(), target_out.hash(), "Secret", "Not for bystander");
    msg.pack(sender_dest, target_out);

    Packet packet(target_out, msg.fullMsg());
    pack_and_inject(packet);

    // Bystander should NOT receive (different destination hash)
    TEST_ASSERT_FALSE(g_received);
}

// ---------------------------------------------------------------------------
// Test 6: Multiple messages in sequence
// ---------------------------------------------------------------------------
void test_lxmf_multiple_messages(void) {
    Identity receiver_id;
    Destination receiver_in(receiver_id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");
    receiver_in.set_packet_callback(on_packet_received);

    Destination receiver_out(receiver_id, Type::Destination::OUT, Type::Destination::SINGLE, "lxmf", "delivery");

    Identity sender_id;
    Destination sender_in(sender_id, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");

    const char* titles[] = {"First", "Second", "Third"};
    const char* bodies[] = {"Body 1", "Body 2", "Body 3"};

    for (int i = 0; i < 3; i++) {
        g_received = false;

        Message msg(sender_in.hash(), receiver_in.hash(), titles[i], bodies[i]);
        msg.pack(sender_in, receiver_out);

        Packet packet(receiver_out, msg.fullMsg());
        pack_and_inject(packet);

        TEST_ASSERT_TRUE_MESSAGE(g_received, titles[i]);

        Message rx(g_received_plaintext);
        rx.unpack();
        TEST_ASSERT_EQUAL_STRING(titles[i], rx.title.c_str());
        TEST_ASSERT_EQUAL_STRING(bodies[i], rx.content.c_str());
    }
}

// ---------------------------------------------------------------------------
// Test 7: fullMsg() wire format structure
// ---------------------------------------------------------------------------
void test_lxmf_wire_format(void) {
    Identity id_a;
    Destination dest_a(id_a, Type::Destination::IN, Type::Destination::SINGLE, "lxmf", "delivery");
    Identity id_b;
    Destination dest_b(id_b, Type::Destination::OUT, Type::Destination::SINGLE, "lxmf", "delivery");

    Message msg(dest_a.hash(), dest_b.hash(), "Wire", "Format");
    msg.pack(dest_a, dest_b);

    Bytes wire = msg.fullMsg();

    // Wire format: dest(16) + src(16) + signature(64) + packed_payload
    TEST_ASSERT_TRUE(wire.size() >= 16 + 16 + 64);

    Bytes wire_dest = wire.left(16);
    TEST_ASSERT_TRUE(wire_dest == dest_b.hash());

    Bytes wire_src = wire.mid(16, 16);
    TEST_ASSERT_TRUE(wire_src == dest_a.hash());

    Bytes wire_sig = wire.mid(32, 64);
    TEST_ASSERT_EQUAL(64, wire_sig.size());
    TEST_ASSERT_TRUE(wire_sig == msg.signature);

    Bytes wire_payload = wire.mid(96);
    TEST_ASSERT_TRUE(wire_payload == msg.packed_payload);
}

int main(int argc, char **argv) {
    init_system();

    UNITY_BEGIN();

    RUN_TEST(test_lxmf_full_roundtrip);
    RUN_TEST(test_lxmf_pack_unpack_roundtrip);
    RUN_TEST(test_lxmf_signature_valid);
    RUN_TEST(test_lxmf_encrypted_on_wire);
    RUN_TEST(test_lxmf_wrong_destination_no_receive);
    RUN_TEST(test_lxmf_multiple_messages);
    RUN_TEST(test_lxmf_wire_format);

    return UNITY_END();
}
