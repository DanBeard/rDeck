"""Cross-compatibility tests with C++ implementation.

These tests verify that Python can decode data encoded by C++ and vice versa.
They use canonical test vectors that both implementations must handle identically.
"""

import pytest
import msgpack
import binascii

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


class TestCanonicalTestVectors:
    """Test vectors that both Python and C++ must handle identically.

    These test vectors define the "contract" between the two implementations.
    If a test fails here, the corresponding C++ test should also fail.
    """

    def test_trust_offer_vector_1(self):
        """Basic TRUST_OFFER with server name and two services."""
        # This is what C++ should produce and Python should decode
        expected_structure = {
            "server_name": "TestServer",
            "services": ["ntp", "search"],
        }

        # Encode using Python
        payload = TrustOfferPayload(
            server_name="TestServer",
            services=["ntp", "search"],
        )
        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)

        # Verify structure
        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

        # Verify roundtrip
        decoded = _decode_payload(MessageType.TRUST_OFFER, encoded)
        assert decoded.server_name == "TestServer"
        assert decoded.services == ["ntp", "search"]

    def test_trust_accept_vector_1(self):
        """Basic TRUST_ACCEPT with device name."""
        expected_structure = {
            "device_name": "rDeck",
        }

        payload = TrustAcceptPayload(device_name="rDeck")
        encoded = _encode_payload(MessageType.TRUST_ACCEPT, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_ntp_request_vector_1(self):
        """NTP request with specific timestamp."""
        expected_structure = {
            "client_timestamp": 1234567890,
        }

        payload = NTPRequestPayload(client_timestamp=1234567890)
        encoded = _encode_payload(MessageType.NTP_REQUEST, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_ntp_response_vector_1(self):
        """NTP response with server and client timestamps."""
        expected_structure = {
            "server_timestamp": 1706825600,
            "client_timestamp": 1000,
        }

        payload = NTPResponsePayload(
            server_timestamp=1706825600,
            client_timestamp=1000,
        )
        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_search_request_vector_1(self):
        """Search request with query and max_results."""
        expected_structure = {
            "query": "python programming",
            "max_results": 5,
        }

        payload = SearchRequestPayload(
            query="python programming",
            max_results=5,
        )
        encoded = _encode_payload(MessageType.SEARCH_REQUEST, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_search_response_vector_1(self):
        """Search response with results."""
        expected_structure = {
            "query": "test",
            "results": [
                {"title": "Title1", "url": "http://example.com", "snippet": "Snippet1"},
            ],
            "error": None,
        }

        payload = SearchResponsePayload(
            query="test",
            results=[
                SearchResult(title="Title1", url="http://example.com", snippet="Snippet1"),
            ],
            error=None,
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_search_response_with_error_vector(self):
        """Search response with error."""
        expected_structure = {
            "query": "failed query",
            "results": [],
            "error": "Network timeout",
        }

        payload = SearchResponsePayload(
            query="failed query",
            results=[],
            error="Network timeout",
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure


class TestMessageTypeCompatibility:
    """Verify message type enum values match protocol spec."""

    # These values are defined in the protocol and must never change
    PROTOCOL_VALUES = {
        "TRUST_OFFER": 0x01,
        "TRUST_ACCEPT": 0x02,
        "TRUST_REVOKE": 0x03,
        "NTP_REQUEST": 0x10,
        "NTP_RESPONSE": 0x11,
        "SEARCH_REQUEST": 0x20,
        "SEARCH_RESPONSE": 0x21,
    }

    def test_all_message_types_match_protocol(self):
        """All message types must have the correct protocol-defined values."""
        assert MessageType.TRUST_OFFER == self.PROTOCOL_VALUES["TRUST_OFFER"]
        assert MessageType.TRUST_ACCEPT == self.PROTOCOL_VALUES["TRUST_ACCEPT"]
        assert MessageType.TRUST_REVOKE == self.PROTOCOL_VALUES["TRUST_REVOKE"]
        assert MessageType.NTP_REQUEST == self.PROTOCOL_VALUES["NTP_REQUEST"]
        assert MessageType.NTP_RESPONSE == self.PROTOCOL_VALUES["NTP_RESPONSE"]
        assert MessageType.SEARCH_REQUEST == self.PROTOCOL_VALUES["SEARCH_REQUEST"]
        assert MessageType.SEARCH_RESPONSE == self.PROTOCOL_VALUES["SEARCH_RESPONSE"]


class TestMsgpackFieldNames:
    """Verify msgpack field names match what C++ expects."""

    def test_trust_offer_field_names(self):
        """TRUST_OFFER must use exactly these field names."""
        payload = TrustOfferPayload(server_name="S", services=["a"])
        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)
        data = msgpack.unpackb(encoded, raw=False)

        # C++ expects these exact keys
        assert "server_name" in data
        assert "services" in data

    def test_trust_accept_field_names(self):
        """TRUST_ACCEPT must use exactly these field names."""
        payload = TrustAcceptPayload(device_name="D")
        encoded = _encode_payload(MessageType.TRUST_ACCEPT, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "device_name" in data

    def test_ntp_request_field_names(self):
        """NTP_REQUEST must use exactly these field names."""
        payload = NTPRequestPayload(client_timestamp=0)
        encoded = _encode_payload(MessageType.NTP_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "client_timestamp" in data

    def test_ntp_response_field_names(self):
        """NTP_RESPONSE must use exactly these field names."""
        payload = NTPResponsePayload(server_timestamp=0, client_timestamp=0)
        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "server_timestamp" in data
        assert "client_timestamp" in data

    def test_search_request_field_names(self):
        """SEARCH_REQUEST must use exactly these field names."""
        payload = SearchRequestPayload(query="q", max_results=1)
        encoded = _encode_payload(MessageType.SEARCH_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "query" in data
        assert "max_results" in data

    def test_search_response_field_names(self):
        """SEARCH_RESPONSE must use exactly these field names."""
        payload = SearchResponsePayload(query="q", results=[], error=None)
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "query" in data
        assert "results" in data
        assert "error" in data

    def test_search_result_field_names(self):
        """Search results must use exactly these field names."""
        payload = SearchResponsePayload(
            query="q",
            results=[SearchResult(title="T", url="U", snippet="S")],
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        result = data["results"][0]
        assert "title" in result
        assert "url" in result
        assert "snippet" in result


class TestLXMFFieldsStructure:
    """Test the structure of LXMF fields dict."""

    def test_fields_dict_keys(self):
        """LXMF fields dict must have these exact keys."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        msg = ServiceMessage(
            msg_type=MessageType.TRUST_OFFER,
            service="trust",
            payload=TrustOfferPayload(server_name="S", services=[]),
            request_id=123,
        )

        fields = encode_service_fields(msg)

        # C++ expects these exact keys in the LXMF fields dict
        assert "msg_type" in fields
        assert "service" in fields
        assert "payload" in fields
        assert "request_id" in fields

    def test_msg_type_is_integer(self):
        """msg_type must be an integer, not enum."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        msg = ServiceMessage(
            msg_type=MessageType.NTP_REQUEST,
            service="ntp",
            payload=NTPRequestPayload(client_timestamp=0),
            request_id=0,
        )

        fields = encode_service_fields(msg)

        assert isinstance(fields["msg_type"], int)
        assert fields["msg_type"] == 0x10

    def test_payload_is_bytes(self):
        """payload must be bytes (msgpack binary)."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        msg = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="D"),
            request_id=0,
        )

        fields = encode_service_fields(msg)

        assert isinstance(fields["payload"], bytes)


class TestDecodeFromCppVectors:
    """Test decoding data that C++ would produce.

    These tests use manually constructed msgpack data to simulate
    what the C++ implementation would produce.
    """

    def test_decode_cpp_trust_offer(self):
        """Decode TRUST_OFFER as C++ would encode it."""
        # Simulate C++ encoded data
        cpp_data = msgpack.packb({
            "server_name": "CppServer",
            "services": ["ntp", "search"],
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.TRUST_OFFER, cpp_data)

        assert decoded.server_name == "CppServer"
        assert decoded.services == ["ntp", "search"]

    def test_decode_cpp_ntp_response(self):
        """Decode NTP_RESPONSE as C++ would encode it."""
        cpp_data = msgpack.packb({
            "server_timestamp": 1706825600,
            "client_timestamp": 12345,
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.NTP_RESPONSE, cpp_data)

        assert decoded.server_timestamp == 1706825600
        assert decoded.client_timestamp == 12345

    def test_decode_cpp_search_response_with_results(self):
        """Decode SEARCH_RESPONSE with results as C++ would encode it."""
        cpp_data = msgpack.packb({
            "query": "cpp query",
            "results": [
                {"title": "CppResult", "url": "http://cpp.example", "snippet": "From C++"},
            ],
            "error": None,
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, cpp_data)

        assert decoded.query == "cpp query"
        assert len(decoded.results) == 1
        assert decoded.results[0].title == "CppResult"
