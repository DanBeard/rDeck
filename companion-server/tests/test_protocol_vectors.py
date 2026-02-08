"""Protocol vector tests - validates Python encoding/decoding against shared test vectors.

These tests ensure that the Python msgpack encoding matches the canonical format
defined in test/data/protocol_vectors.json, which is also validated by C++ tests.

If these tests pass and C++ tests pass, the two implementations are compatible.
"""

import json
import pytest
import msgpack
from pathlib import Path

from companion_server.protocol.messages import (
    MessageType,
    TrustOfferPayload,
    TrustAcceptPayload,
    NTPRequestPayload,
    NTPResponsePayload,
    SearchRequestPayload,
    SearchResponsePayload,
    SearchResult,
)
from companion_server.protocol.serialization import (
    _encode_payload,
    _decode_payload,
)


def load_vectors():
    """Load protocol vectors from shared JSON file."""
    # Try multiple paths to find the vectors file
    paths = [
        Path(__file__).parent.parent.parent / "test" / "data" / "protocol_vectors.json",
        Path(__file__).parent.parent / "test" / "data" / "protocol_vectors.json",
        Path("test/data/protocol_vectors.json"),
        Path("../test/data/protocol_vectors.json"),
    ]

    for path in paths:
        if path.exists():
            with open(path) as f:
                return json.load(f)

    pytest.skip("Protocol vectors file not found")


@pytest.fixture
def vectors():
    """Fixture to load protocol vectors."""
    return load_vectors()


class TestTrustOfferVectors:
    """Tests for TRUST_OFFER message vectors."""

    def test_decode_trust_offer_basic(self, vectors):
        """Decode basic TRUST_OFFER from shared vector."""
        vector = vectors["vectors"]["trust_offer_basic"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.TRUST_OFFER, payload_bytes)

        assert decoded.server_name == "TestServer"
        assert decoded.services == ["ntp", "search"]

    def test_encode_trust_offer_basic(self, vectors):
        """Encode basic TRUST_OFFER and verify against vector."""
        vector = vectors["vectors"]["trust_offer_basic"]
        expected_hex = vector["payload_msgpack_hex"]

        payload = TrustOfferPayload(
            server_name="TestServer",
            services=["ntp", "search"],
        )
        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)

        assert encoded.hex() == expected_hex

    def test_decode_trust_offer_empty_services(self, vectors):
        """Decode TRUST_OFFER with empty services list."""
        vector = vectors["vectors"]["trust_offer_empty_services"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.TRUST_OFFER, payload_bytes)

        assert decoded.server_name == "EmptyServer"
        assert decoded.services == []

    def test_encode_trust_offer_empty_services(self, vectors):
        """Encode TRUST_OFFER with empty services and verify against vector."""
        vector = vectors["vectors"]["trust_offer_empty_services"]
        expected_hex = vector["payload_msgpack_hex"]

        payload = TrustOfferPayload(
            server_name="EmptyServer",
            services=[],
        )
        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)

        assert encoded.hex() == expected_hex

    def test_decode_trust_offer_single_service(self, vectors):
        """Decode TRUST_OFFER with single service."""
        vector = vectors["vectors"]["trust_offer_single_service"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.TRUST_OFFER, payload_bytes)

        assert decoded.server_name == "NTPOnly"
        assert decoded.services == ["ntp"]


class TestTrustAcceptVectors:
    """Tests for TRUST_ACCEPT message vectors."""

    def test_decode_trust_accept_basic(self, vectors):
        """Decode basic TRUST_ACCEPT from shared vector."""
        vector = vectors["vectors"]["trust_accept_basic"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.TRUST_ACCEPT, payload_bytes)

        assert decoded.device_name == "rDeck"

    def test_encode_trust_accept_basic(self, vectors):
        """Encode basic TRUST_ACCEPT and verify against vector."""
        vector = vectors["vectors"]["trust_accept_basic"]
        expected_hex = vector["payload_msgpack_hex"]

        payload = TrustAcceptPayload(device_name="rDeck")
        encoded = _encode_payload(MessageType.TRUST_ACCEPT, payload)

        assert encoded.hex() == expected_hex

    def test_decode_trust_accept_long_name(self, vectors):
        """Decode TRUST_ACCEPT with longer device name."""
        vector = vectors["vectors"]["trust_accept_long_name"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.TRUST_ACCEPT, payload_bytes)

        assert decoded.device_name == "My Personal rDeck Device"

    def test_encode_trust_accept_long_name(self, vectors):
        """Encode TRUST_ACCEPT with long name and verify against vector."""
        vector = vectors["vectors"]["trust_accept_long_name"]
        expected_hex = vector["payload_msgpack_hex"]

        payload = TrustAcceptPayload(device_name="My Personal rDeck Device")
        encoded = _encode_payload(MessageType.TRUST_ACCEPT, payload)

        assert encoded.hex() == expected_hex


class TestNTPRequestVectors:
    """Tests for NTP_REQUEST message vectors."""

    def test_decode_ntp_request_basic(self, vectors):
        """Decode basic NTP_REQUEST from shared vector."""
        vector = vectors["vectors"]["ntp_request_basic"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.NTP_REQUEST, payload_bytes)

        assert decoded.client_timestamp == 1234567890

    def test_encode_ntp_request_basic(self, vectors):
        """Encode basic NTP_REQUEST and verify against vector."""
        vector = vectors["vectors"]["ntp_request_basic"]
        expected_hex = vector["payload_msgpack_hex"]

        payload = NTPRequestPayload(client_timestamp=1234567890)
        encoded = _encode_payload(MessageType.NTP_REQUEST, payload)

        assert encoded.hex() == expected_hex

    def test_decode_ntp_request_zero(self, vectors):
        """Decode NTP_REQUEST with zero timestamp."""
        vector = vectors["vectors"]["ntp_request_zero"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.NTP_REQUEST, payload_bytes)

        assert decoded.client_timestamp == 0

    def test_decode_ntp_request_max_32bit(self, vectors):
        """Decode NTP_REQUEST with max 32-bit timestamp."""
        vector = vectors["vectors"]["ntp_request_max_32bit"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.NTP_REQUEST, payload_bytes)

        assert decoded.client_timestamp == 4294967295


class TestNTPResponseVectors:
    """Tests for NTP_RESPONSE message vectors."""

    def test_decode_ntp_response_basic(self, vectors):
        """Decode basic NTP_RESPONSE from shared vector."""
        vector = vectors["vectors"]["ntp_response_basic"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.NTP_RESPONSE, payload_bytes)

        assert decoded.server_timestamp == 1706825600
        assert decoded.client_timestamp == 1000

    def test_encode_ntp_response_basic(self, vectors):
        """Encode basic NTP_RESPONSE and verify against vector."""
        vector = vectors["vectors"]["ntp_response_basic"]
        expected_hex = vector["payload_msgpack_hex"]

        payload = NTPResponsePayload(
            server_timestamp=1706825600,
            client_timestamp=1000,
        )
        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)

        assert encoded.hex() == expected_hex

    def test_decode_ntp_response_recent(self, vectors):
        """Decode NTP_RESPONSE with recent epoch time."""
        vector = vectors["vectors"]["ntp_response_recent"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.NTP_RESPONSE, payload_bytes)

        assert decoded.server_timestamp == 1700000000
        assert decoded.client_timestamp == 50000


class TestSearchRequestVectors:
    """Tests for SEARCH_REQUEST message vectors."""

    def test_decode_search_request_basic(self, vectors):
        """Decode basic SEARCH_REQUEST from shared vector."""
        vector = vectors["vectors"]["search_request_basic"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.SEARCH_REQUEST, payload_bytes)

        assert decoded.query == "test query"
        assert decoded.max_results == 5

    def test_encode_search_request_basic(self, vectors):
        """Encode basic SEARCH_REQUEST and verify against vector."""
        vector = vectors["vectors"]["search_request_basic"]
        expected_hex = vector["payload_msgpack_hex"]

        payload = SearchRequestPayload(query="test query", max_results=5)
        encoded = _encode_payload(MessageType.SEARCH_REQUEST, payload)

        assert encoded.hex() == expected_hex

    def test_decode_search_request_custom_max(self, vectors):
        """Decode SEARCH_REQUEST with custom max_results."""
        vector = vectors["vectors"]["search_request_custom_max"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.SEARCH_REQUEST, payload_bytes)

        assert decoded.query == "python programming"
        assert decoded.max_results == 10


class TestSearchResponseVectors:
    """Tests for SEARCH_RESPONSE message vectors."""

    def test_decode_search_response_with_results(self, vectors):
        """Decode SEARCH_RESPONSE with results from shared vector."""
        vector = vectors["vectors"]["search_response_with_results"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, payload_bytes)

        assert decoded.query == "test"
        assert len(decoded.results) == 2
        assert decoded.results[0].title == "Result 1"
        assert decoded.results[0].url == "https://example.com/1"
        assert decoded.results[0].snippet == "First result"
        assert decoded.results[1].title == "Result 2"
        assert decoded.results[1].url == "https://example.com/2"
        assert decoded.results[1].snippet == "Second result"
        assert decoded.error is None

    def test_encode_search_response_with_results(self, vectors):
        """Encode SEARCH_RESPONSE with results and verify against vector."""
        vector = vectors["vectors"]["search_response_with_results"]
        expected_hex = vector["payload_msgpack_hex"]

        payload = SearchResponsePayload(
            query="test",
            results=[
                SearchResult(title="Result 1", url="https://example.com/1", snippet="First result"),
                SearchResult(title="Result 2", url="https://example.com/2", snippet="Second result"),
            ],
            error=None,
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)

        assert encoded.hex() == expected_hex

    def test_decode_search_response_empty(self, vectors):
        """Decode SEARCH_RESPONSE with no results."""
        vector = vectors["vectors"]["search_response_empty"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, payload_bytes)

        assert decoded.query == "no results query"
        assert decoded.results == []
        assert decoded.error is None

    def test_decode_search_response_with_error(self, vectors):
        """Decode SEARCH_RESPONSE with error."""
        vector = vectors["vectors"]["search_response_with_error"]
        hex_data = vector["payload_msgpack_hex"]
        payload_bytes = bytes.fromhex(hex_data)

        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, payload_bytes)

        assert decoded.query == "failed query"
        assert decoded.results == []
        assert decoded.error == "Network timeout"


class TestMessageTypeValues:
    """Tests that message type enum values match the shared vectors."""

    def test_message_type_trust_offer(self, vectors):
        assert MessageType.TRUST_OFFER == vectors["message_types"]["TRUST_OFFER"]

    def test_message_type_trust_accept(self, vectors):
        assert MessageType.TRUST_ACCEPT == vectors["message_types"]["TRUST_ACCEPT"]

    def test_message_type_trust_revoke(self, vectors):
        assert MessageType.TRUST_REVOKE == vectors["message_types"]["TRUST_REVOKE"]

    def test_message_type_ntp_request(self, vectors):
        assert MessageType.NTP_REQUEST == vectors["message_types"]["NTP_REQUEST"]

    def test_message_type_ntp_response(self, vectors):
        assert MessageType.NTP_RESPONSE == vectors["message_types"]["NTP_RESPONSE"]

    def test_message_type_search_request(self, vectors):
        assert MessageType.SEARCH_REQUEST == vectors["message_types"]["SEARCH_REQUEST"]

    def test_message_type_search_response(self, vectors):
        assert MessageType.SEARCH_RESPONSE == vectors["message_types"]["SEARCH_RESPONSE"]


class TestRoundTrip:
    """Round-trip tests: encode in Python, decode, verify matches original."""

    def test_roundtrip_trust_offer(self):
        """Round-trip test for TRUST_OFFER."""
        original = TrustOfferPayload(
            server_name="TestServer",
            services=["ntp", "search", "weather"],
        )
        encoded = _encode_payload(MessageType.TRUST_OFFER, original)
        decoded = _decode_payload(MessageType.TRUST_OFFER, encoded)

        assert decoded.server_name == original.server_name
        assert decoded.services == original.services

    def test_roundtrip_ntp_response(self):
        """Round-trip test for NTP_RESPONSE."""
        original = NTPResponsePayload(
            server_timestamp=1706825600,
            client_timestamp=12345,
        )
        encoded = _encode_payload(MessageType.NTP_RESPONSE, original)
        decoded = _decode_payload(MessageType.NTP_RESPONSE, encoded)

        assert decoded.server_timestamp == original.server_timestamp
        assert decoded.client_timestamp == original.client_timestamp

    def test_roundtrip_search_response(self):
        """Round-trip test for SEARCH_RESPONSE."""
        original = SearchResponsePayload(
            query="test query",
            results=[
                SearchResult(title="T1", url="http://u1.com", snippet="S1"),
                SearchResult(title="T2", url="http://u2.com", snippet="S2"),
            ],
            error=None,
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, original)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)

        assert decoded.query == original.query
        assert len(decoded.results) == len(original.results)
        for i, result in enumerate(decoded.results):
            assert result.title == original.results[i].title
            assert result.url == original.results[i].url
            assert result.snippet == original.results[i].snippet
