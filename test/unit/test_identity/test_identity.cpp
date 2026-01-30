/**
 * Unit tests for RNS::Identity class
 */

#include <unity.h>
#include "Identity.h"
#include "Bytes.h"
#include "Type.h"

using namespace RNS;

void setUp(void) {}
void tearDown(void) {}

// Key generation creates valid key material
void test_identity_creates_keys(void) {
    Identity id;
    TEST_ASSERT_TRUE(id);

    Bytes pub = id.get_public_key();
    Bytes prv = id.get_private_key();

    // Public key: 32 bytes X25519 + 32 bytes Ed25519 = 64
    TEST_ASSERT_EQUAL(Type::Identity::KEYSIZE / 8, pub.size());
    // Private key: 32 bytes X25519 + 32 bytes Ed25519 = 64
    TEST_ASSERT_EQUAL(Type::Identity::KEYSIZE / 8, prv.size());
}

// Hash is correct size (truncated)
void test_identity_hash_size(void) {
    Identity id;
    Bytes hash = id.hash();
    TEST_ASSERT_EQUAL(Type::Reticulum::TRUNCATED_HASHLENGTH / 8, hash.size());
}

// Hex hash matches hash
void test_identity_hexhash(void) {
    Identity id;
    std::string hexhash = id.hexhash();
    Bytes hash = id.hash();
    TEST_ASSERT_EQUAL_STRING(hash.toHex().c_str(), hexhash.c_str());
}

// Two identities have different keys
void test_identity_uniqueness(void) {
    Identity id1;
    Identity id2;
    TEST_ASSERT_FALSE(id1.get_public_key() == id2.get_public_key());
    TEST_ASSERT_FALSE(id1.hash() == id2.hash());
}

// Sign/verify roundtrip
void test_identity_sign_verify(void) {
    Identity id;
    Bytes message("test message to sign");
    Bytes signature = id.sign(message);

    TEST_ASSERT_EQUAL(Type::Identity::SIGLENGTH / 8, signature.size());
    TEST_ASSERT_TRUE(id.validate(signature, message));
}

// Verify fails with wrong message
void test_identity_verify_wrong_message(void) {
    Identity id;
    Bytes message("correct message");
    Bytes signature = id.sign(message);

    Bytes wrong("wrong message");
    TEST_ASSERT_FALSE(id.validate(signature, wrong));
}

// Verify fails with wrong identity
void test_identity_verify_wrong_identity(void) {
    Identity id1;
    Identity id2;
    Bytes message("test");
    Bytes signature = id1.sign(message);

    TEST_ASSERT_FALSE(id2.validate(signature, message));
}

// Encrypt/decrypt roundtrip
void test_identity_encrypt_decrypt(void) {
    Identity id;
    Bytes plaintext("secret message for encryption test");
    Bytes ciphertext = id.encrypt(plaintext);

    TEST_ASSERT_TRUE(ciphertext.size() > 0);
    TEST_ASSERT_FALSE(plaintext == ciphertext);

    // Decrypt with empty ratchets
    std::vector<Bytes> empty_ratchets;
    Bytes decrypted = id.decrypt(ciphertext, empty_ratchets);
    TEST_ASSERT_TRUE(plaintext == decrypted);
}

// Encrypt produces different ciphertext each time (due to random IV)
void test_identity_encrypt_nondeterministic(void) {
    Identity id;
    Bytes plaintext("same plaintext");
    Bytes ct1 = id.encrypt(plaintext);
    Bytes ct2 = id.encrypt(plaintext);
    TEST_ASSERT_FALSE(ct1 == ct2);
}

// Load public key from exported bytes
void test_identity_export_import_public_key(void) {
    Identity id1;
    Bytes pub = id1.get_public_key();

    Identity id2(false); // don't create keys
    id2.load_public_key(pub);

    // Should be able to verify signatures made by id1
    Bytes message("verify me");
    Bytes signature = id1.sign(message);
    TEST_ASSERT_TRUE(id2.validate(signature, message));
}

// Load private key from exported bytes
void test_identity_export_import_private_key(void) {
    Identity id1;
    Bytes prv = id1.get_private_key();

    Identity id2(false);
    id2.load_private_key(prv);

    // id2 should produce same signatures
    Bytes message("test import");
    Bytes sig1 = id1.sign(message);
    Bytes sig2 = id2.sign(message);
    TEST_ASSERT_TRUE(sig1 == sig2);
}

// full_hash produces 32-byte SHA-256
void test_identity_full_hash(void) {
    Bytes data("hash me");
    Bytes hash = Identity::full_hash(data);
    TEST_ASSERT_EQUAL(32, hash.size());
}

// truncated_hash produces correct size
void test_identity_truncated_hash(void) {
    Bytes data("truncate me");
    Bytes hash = Identity::truncated_hash(data);
    TEST_ASSERT_EQUAL(Type::Identity::TRUNCATED_HASHLENGTH / 8, hash.size());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_identity_creates_keys);
    RUN_TEST(test_identity_hash_size);
    RUN_TEST(test_identity_hexhash);
    RUN_TEST(test_identity_uniqueness);
    RUN_TEST(test_identity_sign_verify);
    RUN_TEST(test_identity_verify_wrong_message);
    RUN_TEST(test_identity_verify_wrong_identity);
    RUN_TEST(test_identity_encrypt_decrypt);
    RUN_TEST(test_identity_encrypt_nondeterministic);
    RUN_TEST(test_identity_export_import_public_key);
    RUN_TEST(test_identity_export_import_private_key);
    RUN_TEST(test_identity_full_hash);
    RUN_TEST(test_identity_truncated_hash);

    return UNITY_END();
}
