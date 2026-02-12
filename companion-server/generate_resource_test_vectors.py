#!/usr/bin/env python3
"""
Generate test vectors for Resource advertisement and transfer testing.
Mimics what Python RNS does when creating and advertising a Resource.
"""

import hashlib
import math
from RNS.vendor import umsgpack

# ============================================================
# Constants (matching RNS)
# ============================================================
RANDOM_HASH_SIZE = 4
MAPHASH_LEN = 4
SDU = 464  # RNS.Packet.MDU = 464

# ============================================================
# Test data
# ============================================================
original_data = b"Hello from Python resource!"  # 26 bytes
random_hash = bytes([0xDE, 0xAD, 0xBE, 0xEF])  # Fixed 4 bytes

# In RNS, a resource prepends a 4-byte random nonce to the data.
# The "data" field on the Resource after construction (before encryption) is:
#   nonce (4 bytes) + original_data (26 bytes) = 30 bytes
#
# For unit-test purposes (simulating unencrypted / testing the advertisement
# parsing independently of link encryption), we treat the transfer data as
# unencrypted.  The nonce is typically random, but for reproducibility we
# use fixed bytes.
nonce = bytes([0xCA, 0xFE, 0xBA, 0xBE])  # Fixed 4-byte nonce
transfer_data = nonce + original_data  # 30 bytes total

# ============================================================
# Sizes
# ============================================================
transfer_size = len(transfer_data)  # t = 30
data_size = len(transfer_data)      # d = 30 (total_size = metadata_size + data_size;
                                    #         in RNS this is len(resource_data) which
                                    #         is nonce+original for uncompressed single segment)
# NOTE: In real RNS, "d" (total_size) = data_size + metadata_size. For our
# test with no metadata, d = data_size.  And data_size = len(uncompressed_data)
# which is the original_data length.  BUT the `total_size` set in __init__
# is `data_size + metadata_size` = len(original_data) + 0 = 26.
# Let's match exactly what RNS does:
data_size = len(original_data)  # d = 26 (the uncompressed data size)
# transfer_size remains 30 (encrypted/transfer payload size)

num_parts = math.ceil(transfer_size / SDU)  # ceil(30/464) = 1

# ============================================================
# Hashes (matching RNS Resource.__init__)
# ============================================================
# In RNS:
#   self.data = random_nonce + uncompressed_data  (then encrypted)
#   For hash computation, "data" at line 438 is the local variable which
#   at that point equals self.uncompressed_data (the original bytes passed in,
#   possibly with metadata prepended).
#
# Looking at RNS code more carefully:
#   line 384: self.uncompressed_data = data  (where data = resource_data if no metadata)
#   line 408: self.data = b"" + get_random_hash()[:4] + self.uncompressed_data
#   line 424: self.data = self.link.encrypt(self.data)  -> encrypted
#   line 438: self.hash = full_hash(data + self.random_hash)
#
# WAIT - line 438 uses the local variable `data`, NOT `self.data`.
# At line 438, the local `data` has been reassigned at line 451 in the
# hashmap loop:  data = self.data[i*self.sdu:(i+1)*self.sdu]
#
# Let me re-read the flow more carefully...
# Actually line 438 is BEFORE the loop at 450.
# At line 438, the local `data` is:
#   - If has_metadata: self.metadata + resource_data (line 330)
#   - Else: resource_data (line 331)
# So `data` at line 438 = resource_data (the original bytes passed to Resource())
# with metadata prepended if any.
#
# For our test with no metadata:
#   data (at hash computation) = original_data = b"Hello from Python resource!"

hash_input_data = original_data  # This is what RNS uses for hash computation

# resource.hash = SHA256(data + random_hash)
resource_hash = hashlib.sha256(hash_input_data + random_hash).digest()

# resource.original_hash = resource.hash (single segment, no original_hash passed)
original_hash = resource_hash

# resource.expected_proof = SHA256(data + hash)
expected_proof = hashlib.sha256(hash_input_data + resource_hash).digest()

# ============================================================
# Hashmap
# ============================================================
# map_hash = full_hash(part_data + random_hash)[:MAPHASH_LEN]
# where part_data = self.data[i*sdu:(i+1)*sdu] = the encrypted transfer data
# For our test (unencrypted), part_data = transfer_data
part_data = transfer_data  # The full 30 bytes (single part, fits in one SDU)
map_hash = hashlib.sha256(part_data + random_hash).digest()[:MAPHASH_LEN]

# The hashmap is the concatenation of all map hashes
hashmap = map_hash  # Single part, so just one map hash

# ============================================================
# Flags
# ============================================================
# f = 0x00 | has_metadata<<5 | is_response<<4 | is_request<<3 | split<<2 | compressed<<1 | encrypted
# For a plain resource (not encrypted in our test, not compressed, not split,
# no metadata, not request, not response):
encrypted = 0
compressed = 0
split = 0
is_request = 0
is_response = 0
has_metadata = 0
flags = (has_metadata << 5) | (is_response << 4) | (is_request << 3) | (split << 2) | (compressed << 1) | encrypted

# ============================================================
# Advertisement dictionary
# ============================================================
segment_index = 1
total_segments = 1
request_id = None

adv_dict = {
    "t": transfer_size,    # 30
    "d": data_size,        # 26
    "n": num_parts,        # 1
    "h": resource_hash,    # 32 bytes
    "r": random_hash,      # 4 bytes
    "o": original_hash,    # 32 bytes (same as h for single segment)
    "i": segment_index,    # 1
    "l": total_segments,   # 1
    "q": request_id,       # None
    "f": flags,            # 0x00
    "m": hashmap,          # 4 bytes (single map hash)
}

# Pack using umsgpack (same as RNS uses)
packed_adv = umsgpack.packb(adv_dict)

# ============================================================
# Output
# ============================================================
def bytes_to_cpp_array(data, name="data"):
    """Format bytes as a C++ byte array literal."""
    hex_bytes = ", ".join(f"0x{b:02X}" for b in data)
    return f"const uint8_t {name}[] = {{{hex_bytes}}};"

def bytes_to_hex(data):
    return data.hex()

print("=" * 70)
print("RESOURCE ADVERTISEMENT TEST VECTORS")
print("=" * 70)
print()

print("--- Input Data ---")
print(f"original_data ({len(original_data)} bytes): {bytes_to_hex(original_data)}")
print(f"  as string: \"{original_data.decode()}\"")
print(f"nonce ({len(nonce)} bytes): {bytes_to_hex(nonce)}")
print(f"random_hash ({len(random_hash)} bytes): {bytes_to_hex(random_hash)}")
print()

print("--- Computed Values ---")
print(f"transfer_data ({len(transfer_data)} bytes): {bytes_to_hex(transfer_data)}")
print(f"  = nonce + original_data")
print(f"transfer_size (t): {transfer_size}")
print(f"data_size (d): {data_size}")
print(f"num_parts (n): {num_parts}")
print(f"segment_index (i): {segment_index}")
print(f"total_segments (l): {total_segments}")
print(f"flags (f): 0x{flags:02X}")
print(f"request_id (q): {request_id}")
print()

print("--- Hashes ---")
print(f"resource_hash (h) = SHA256(original_data + random_hash):")
print(f"  {bytes_to_hex(resource_hash)}")
print(f"original_hash (o) = same as h for single segment:")
print(f"  {bytes_to_hex(original_hash)}")
print(f"expected_proof = SHA256(original_data + resource_hash):")
print(f"  {bytes_to_hex(expected_proof)}")
print()

print("--- Hashmap ---")
print(f"part_data ({len(part_data)} bytes) = transfer_data:")
print(f"  {bytes_to_hex(part_data)}")
print(f"map_hash = SHA256(part_data + random_hash)[:4]:")
print(f"  {bytes_to_hex(map_hash)}")
print(f"hashmap ({len(hashmap)} bytes): {bytes_to_hex(hashmap)}")
print()

print("--- Packed Advertisement ---")
print(f"packed size: {len(packed_adv)} bytes")
print(f"packed hex: {bytes_to_hex(packed_adv)}")
print()

print("--- C++ Byte Arrays ---")
print()
print(f"// Original data: \"{original_data.decode()}\"")
print(bytes_to_cpp_array(original_data, "original_data"))
print(f"const size_t original_data_len = {len(original_data)};")
print()
print(f"// Nonce (prepended to data to form transfer payload)")
print(bytes_to_cpp_array(nonce, "nonce"))
print()
print(f"// Random hash used for resource hashing")
print(bytes_to_cpp_array(random_hash, "random_hash"))
print(f"const size_t random_hash_len = {len(random_hash)};")
print()
print(f"// Transfer data = nonce + original_data ({len(transfer_data)} bytes)")
print(bytes_to_cpp_array(transfer_data, "transfer_data"))
print(f"const size_t transfer_data_len = {len(transfer_data)};")
print()
print(f"// Resource hash = SHA256(original_data + random_hash)")
print(bytes_to_cpp_array(resource_hash, "resource_hash"))
print(f"const size_t resource_hash_len = {len(resource_hash)};")
print()
print(f"// Original hash (same as resource_hash for single segment)")
print(bytes_to_cpp_array(original_hash, "original_hash"))
print()
print(f"// Expected proof = SHA256(original_data + resource_hash)")
print(bytes_to_cpp_array(expected_proof, "expected_proof"))
print(f"const size_t expected_proof_len = {len(expected_proof)};")
print()
print(f"// Map hash for part 0 = SHA256(transfer_data + random_hash)[:4]")
print(bytes_to_cpp_array(map_hash, "map_hash_part0"))
print()
print(f"// Full hashmap (concatenation of all map hashes)")
print(bytes_to_cpp_array(hashmap, "hashmap_data"))
print(f"const size_t hashmap_len = {len(hashmap)};")
print()
print(f"// Packed ResourceAdvertisement (umsgpack)")
print(bytes_to_cpp_array(packed_adv, "packed_advertisement"))
print(f"const size_t packed_advertisement_len = {len(packed_adv)};")
print()

# ============================================================
# Verification: unpack and verify round-trip
# ============================================================
print("--- Verification (unpack round-trip) ---")
unpacked = umsgpack.unpackb(packed_adv)
assert unpacked["t"] == transfer_size, f"t mismatch: {unpacked['t']} != {transfer_size}"
assert unpacked["d"] == data_size, f"d mismatch: {unpacked['d']} != {data_size}"
assert unpacked["n"] == num_parts, f"n mismatch: {unpacked['n']} != {num_parts}"
assert unpacked["h"] == resource_hash, f"h mismatch"
assert unpacked["r"] == random_hash, f"r mismatch"
assert unpacked["o"] == original_hash, f"o mismatch"
assert unpacked["m"] == hashmap, f"m mismatch"
assert unpacked["f"] == flags, f"f mismatch: {unpacked['f']} != {flags}"
assert unpacked["i"] == segment_index, f"i mismatch"
assert unpacked["l"] == total_segments, f"l mismatch"
assert unpacked["q"] == request_id, f"q mismatch"
print("All assertions passed! Round-trip verified.")
print()

# ============================================================
# Also generate a part packet test vector
# ============================================================
print("=" * 70)
print("RESOURCE PART TEST VECTORS")
print("=" * 70)
print()
print("Part 0 (the only part):")
print(f"  part_data ({len(part_data)} bytes): {bytes_to_hex(part_data)}")
print(f"  map_hash: {bytes_to_hex(map_hash)}")
print()
print(f"// Part 0 data (same as transfer_data for single-part resource)")
print(bytes_to_cpp_array(part_data, "part0_data"))
print(f"const size_t part0_data_len = {len(part_data)};")
print()

# ============================================================
# Resource proof test vector
# ============================================================
print("=" * 70)
print("RESOURCE PROOF TEST VECTORS")
print("=" * 70)
print()
print("After receiving all parts, the receiver assembles the data and")
print("computes a proof to send back to the sender.")
print()
print("In RNS, the proof is computed as:")
print("  proof = SHA256(assembled_data + resource_hash)")
print("  where assembled_data is the reassembled transfer data")
print("  (but after decryption, which we skip in tests)")
print()
print("For the SENDER side, expected_proof was computed at creation time as:")
print("  expected_proof = SHA256(original_data_with_metadata + resource_hash)")
print("  where original_data_with_metadata = metadata + original_data (no metadata here)")
print()
print("NOTE: The proof the RECEIVER sends is over the reassembled (decrypted) data.")
print("Since we're testing without encryption, the receiver's assembled data after")
print("stripping the nonce would be original_data. But actually in RNS the proof")
print("is computed over the FULL assembled data including the nonce prefix.")
print()

# Let me check exactly how the receiver computes the proof
# In Resource.py, after assembly:
#   self.data = assembled data (all parts concatenated)
#   Then: the data is decrypted: self.data = self.link.decrypt(self.data)
#   Then: self.data[0:RANDOM_HASH_SIZE] is the nonce, rest is the payload
#   The proof: proof = full_hash(self.data + self.hash)
#   Wait, let me check...
print("Looking at RNS proof computation in validate_proof()...")
print("The sender validates the proof by checking:")
print("  received_proof == self.expected_proof")
print(f"  where self.expected_proof = SHA256(data + self.hash)")
print(f"  and 'data' = original_data (the local var at hash computation time)")
print()
print(f"expected_proof hex: {bytes_to_hex(expected_proof)}")
print()
print(bytes_to_cpp_array(expected_proof, "expected_proof"))
print()

# ============================================================
# Now also show what the RECEIVER would compute as proof
# ============================================================
# In RNS Resource.__validate():
#   After assembly, decrypted data = nonce + original_data
#   The receiver doesn't directly compute the proof via __validate
#   Let me look at the actual code path for receiver proof...
#
# Actually, looking at RNS code: the receiver sends the resource hash
# as proof, and the link layer wraps it. Let me check...

print("=" * 70)
print("ADDITIONAL CONTEXT")
print("=" * 70)
print()
print(f"SDU (max bytes per part): {SDU}")
print(f"MAPHASH_LEN: {MAPHASH_LEN}")
print(f"RANDOM_HASH_SIZE: {RANDOM_HASH_SIZE}")
print(f"HASHMAP_MAX_LEN: 74 (from RNS)")
print(f"ResourceAdvertisement.OVERHEAD: 134 (from RNS)")
print()

# Print the msgpack structure for debugging
print("--- Msgpack Structure Analysis ---")
print("Advertisement dictionary keys and types:")
for k, v in adv_dict.items():
    if isinstance(v, bytes):
        print(f"  '{k}': bytes({len(v)}) = {bytes_to_hex(v)}")
    elif v is None:
        print(f"  '{k}': null")
    else:
        print(f"  '{k}': {type(v).__name__} = {v}")
print()

# Also verify SHA256 computations step by step
print("--- SHA256 Computation Verification ---")
print(f"SHA256 input for resource_hash:")
print(f"  original_data + random_hash = {bytes_to_hex(hash_input_data + random_hash)}")
print(f"  = {bytes_to_hex(resource_hash)}")
print()
print(f"SHA256 input for expected_proof:")
print(f"  original_data + resource_hash = {bytes_to_hex(hash_input_data + resource_hash)}")
print(f"  = {bytes_to_hex(expected_proof)}")
print()
print(f"SHA256 input for map_hash:")
print(f"  part_data + random_hash = {bytes_to_hex(part_data + random_hash)}")
print(f"  full: {hashlib.sha256(part_data + random_hash).hexdigest()}")
print(f"  truncated to 4 bytes: {bytes_to_hex(map_hash)}")
