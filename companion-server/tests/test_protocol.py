"""Tests for protocol message serialization/deserialization.

These tests define canonical test vectors that both Python and C++ implementations
must agree on to ensure cross-platform compatibility.
"""

import pytest
import msgpack

from companion_server.protocol.messages import (
    MessageType,
    ServiceMessage,
    TrustOfferPayload,
    TrustAcceptPayload,
    TrustRevokePayload,
    NTPRequestPayload,
    NTPResponsePayload,
    SearchRequestPayload,
    SearchResponsePayload,
    SearchResult,
    PropSyncRequestPayload,
    PropSyncResponsePayload,
    PropMsgDeliverPayload,
    PropSubmitRequestPayload,
    PropSubmitResponsePayload,
)
from companion_server.protocol.serialization import (
    encode_service_fields,
    decode_service_fields,
    _encode_payload,
    _decode_payload,
)


class TestMessageTypes:
    """Test message type enum values match the protocol spec."""

    def test_trust_offer_value(self):
        assert MessageType.TRUST_OFFER == 0x01

    def test_trust_accept_value(self):
        assert MessageType.TRUST_ACCEPT == 0x02

    def test_trust_revoke_value(self):
        assert MessageType.TRUST_REVOKE == 0x03

    def test_ntp_request_value(self):
        assert MessageType.NTP_REQUEST == 0x10

    def test_ntp_response_value(self):
        assert MessageType.NTP_RESPONSE == 0x11

    def test_search_request_value(self):
        assert MessageType.SEARCH_REQUEST == 0x20

    def test_search_response_value(self):
        assert MessageType.SEARCH_RESPONSE == 0x21


class TestTrustOfferPayload:
    """Test TrustOfferPayload serialization."""

    def test_encode_decode_roundtrip(self):
        payload = TrustOfferPayload(
            server_name="Test Server",
            services=["ntp", "search"],
        )

        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)
        decoded = _decode_payload(MessageType.TRUST_OFFER, encoded)

        assert decoded.server_name == "Test Server"
        assert decoded.services == ["ntp", "search"]

    def test_canonical_encoding(self):
        """Test that encoding produces expected msgpack structure."""
        payload = TrustOfferPayload(
            server_name="TestServer",
            services=["ntp"],
        )

        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert data["server_name"] == "TestServer"
        assert data["services"] == ["ntp"]

    def test_empty_services_list(self):
        payload = TrustOfferPayload(server_name="Server", services=[])

        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)
        decoded = _decode_payload(MessageType.TRUST_OFFER, encoded)

        assert decoded.services == []


class TestTrustAcceptPayload:
    """Test TrustAcceptPayload serialization."""

    def test_encode_decode_roundtrip(self):
        payload = TrustAcceptPayload(device_name="My rDeck")

        encoded = _encode_payload(MessageType.TRUST_ACCEPT, payload)
        decoded = _decode_payload(MessageType.TRUST_ACCEPT, encoded)

        assert decoded.device_name == "My rDeck"

    def test_canonical_encoding(self):
        payload = TrustAcceptPayload(device_name="rDeck123")

        encoded = _encode_payload(MessageType.TRUST_ACCEPT, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert data["device_name"] == "rDeck123"


class TestNTPPayloads:
    """Test NTP request/response serialization."""

    def test_request_roundtrip(self):
        payload = NTPRequestPayload(client_timestamp=1234567890)

        encoded = _encode_payload(MessageType.NTP_REQUEST, payload)
        decoded = _decode_payload(MessageType.NTP_REQUEST, encoded)

        assert decoded.client_timestamp == 1234567890

    def test_response_roundtrip(self):
        payload = NTPResponsePayload(
            server_timestamp=1706825600,
            client_timestamp=1234567890,
        )

        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)
        decoded = _decode_payload(MessageType.NTP_RESPONSE, encoded)

        assert decoded.server_timestamp == 1706825600
        assert decoded.client_timestamp == 1234567890

    def test_request_canonical_encoding(self):
        payload = NTPRequestPayload(client_timestamp=12345)

        encoded = _encode_payload(MessageType.NTP_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert data["client_timestamp"] == 12345

    def test_response_canonical_encoding(self):
        payload = NTPResponsePayload(server_timestamp=99999, client_timestamp=88888)

        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert data["server_timestamp"] == 99999
        assert data["client_timestamp"] == 88888


class TestSearchPayloads:
    """Test search request/response serialization."""

    def test_request_roundtrip(self):
        payload = SearchRequestPayload(query="test query", max_results=3)

        encoded = _encode_payload(MessageType.SEARCH_REQUEST, payload)
        decoded = _decode_payload(MessageType.SEARCH_REQUEST, encoded)

        assert decoded.query == "test query"
        assert decoded.max_results == 3

    def test_request_default_max_results(self):
        payload = SearchRequestPayload(query="hello")

        encoded = _encode_payload(MessageType.SEARCH_REQUEST, payload)
        decoded = _decode_payload(MessageType.SEARCH_REQUEST, encoded)

        assert decoded.max_results == 5

    def test_response_with_results(self):
        payload = SearchResponsePayload(
            query="python",
            results=[
                SearchResult(title="Python.org", url="https://python.org", snippet="Official site"),
                SearchResult(title="Learn Python", url="https://learn.python.org", snippet="Tutorial"),
            ],
            error=None,
        )

        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)

        assert decoded.query == "python"
        assert len(decoded.results) == 2
        assert decoded.results[0].title == "Python.org"
        assert decoded.results[0].url == "https://python.org"
        assert decoded.results[1].snippet == "Tutorial"
        assert decoded.error is None

    def test_response_with_error(self):
        payload = SearchResponsePayload(
            query="test",
            results=[],
            error="Network error",
        )

        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)

        assert decoded.query == "test"
        assert decoded.results == []
        assert decoded.error == "Network error"

    def test_response_empty_results(self):
        payload = SearchResponsePayload(query="obscure query", results=[], error=None)

        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)

        assert decoded.results == []


class TestServiceMessage:
    """Test full ServiceMessage encoding/decoding."""

    def test_trust_offer_message(self):
        payload = TrustOfferPayload(server_name="Home Server", services=["ntp", "search"])
        msg = ServiceMessage(
            msg_type=MessageType.TRUST_OFFER,
            service="trust",
            payload=payload,
            request_id=42,
        )

        fields = encode_service_fields(msg)

        assert fields["msg_type"] == 0x01
        assert fields["service"] == "trust"
        assert fields["request_id"] == 42
        assert isinstance(fields["payload"], bytes)

        decoded = decode_service_fields(fields)

        assert decoded.msg_type == MessageType.TRUST_OFFER
        assert decoded.service == "trust"
        assert decoded.request_id == 42
        assert decoded.payload.server_name == "Home Server"
        assert decoded.payload.services == ["ntp", "search"]

    def test_ntp_request_message(self):
        payload = NTPRequestPayload(client_timestamp=9999)
        msg = ServiceMessage(
            msg_type=MessageType.NTP_REQUEST,
            service="ntp",
            payload=payload,
            request_id=100,
        )

        fields = encode_service_fields(msg)
        decoded = decode_service_fields(fields)

        assert decoded.msg_type == MessageType.NTP_REQUEST
        assert decoded.service == "ntp"
        assert decoded.payload.client_timestamp == 9999

    def test_search_response_message(self):
        payload = SearchResponsePayload(
            query="rust",
            results=[SearchResult(title="Rust Lang", url="https://rust-lang.org", snippet="Systems language")],
        )
        msg = ServiceMessage(
            msg_type=MessageType.SEARCH_RESPONSE,
            service="search",
            payload=payload,
            request_id=200,
        )

        fields = encode_service_fields(msg)
        decoded = decode_service_fields(fields)

        assert decoded.msg_type == MessageType.SEARCH_RESPONSE
        assert len(decoded.payload.results) == 1


class TestCrossCompatibilityVectors:
    """Test vectors that must be compatible with C++ implementation.

    These tests produce canonical byte sequences that the C++ tests
    will verify they can decode correctly.
    """

    def test_trust_offer_vector(self):
        """Canonical TRUST_OFFER encoding."""
        payload = TrustOfferPayload(
            server_name="TestServer",
            services=["ntp", "search"],
        )

        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)

        # Verify structure
        data = msgpack.unpackb(encoded, raw=False)
        assert data == {
            "server_name": "TestServer",
            "services": ["ntp", "search"],
        }

    def test_ntp_response_vector(self):
        """Canonical NTP_RESPONSE encoding."""
        payload = NTPResponsePayload(
            server_timestamp=1706825600,
            client_timestamp=1000,
        )

        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)

        data = msgpack.unpackb(encoded, raw=False)
        assert data == {
            "server_timestamp": 1706825600,
            "client_timestamp": 1000,
        }

    def test_search_response_vector(self):
        """Canonical SEARCH_RESPONSE encoding."""
        payload = SearchResponsePayload(
            query="test",
            results=[
                SearchResult(title="Title1", url="http://example.com", snippet="Snippet1"),
            ],
            error=None,
        )

        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)

        data = msgpack.unpackb(encoded, raw=False)
        assert data["query"] == "test"
        assert len(data["results"]) == 1
        assert data["results"][0]["title"] == "Title1"

    def test_full_message_fields_structure(self):
        """Test the full LXMF fields dict structure."""
        payload = TrustAcceptPayload(device_name="rDeck")
        msg = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=payload,
            request_id=12345,
        )

        fields = encode_service_fields(msg)

        # These exact field names must match C++ expectations
        assert "msg_type" in fields
        assert "service" in fields
        assert "payload" in fields
        assert "request_id" in fields

        assert fields["msg_type"] == 0x02
        assert fields["service"] == "trust"
        assert fields["request_id"] == 12345


class TestPropagationMessageTypes:
    """Test propagation message type enum values."""

    def test_prop_sync_request_value(self):
        assert MessageType.PROP_SYNC_REQUEST == 0x40

    def test_prop_sync_response_value(self):
        assert MessageType.PROP_SYNC_RESPONSE == 0x41

    def test_prop_msg_deliver_value(self):
        assert MessageType.PROP_MSG_DELIVER == 0x42

    def test_prop_submit_request_value(self):
        assert MessageType.PROP_SUBMIT_REQUEST == 0x43

    def test_prop_submit_response_value(self):
        assert MessageType.PROP_SUBMIT_RESPONSE == 0x44


class TestPropSyncRequestPayload:
    """Test PropSyncRequestPayload serialization."""

    def test_encode_decode_roundtrip(self):
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=bytes.fromhex("abcdef0123456789abcdef0123456789"),
            known_ids=[bytes(32)],
            max_messages=5,
        )

        encoded = _encode_payload(MessageType.PROP_SYNC_REQUEST, payload)
        decoded = _decode_payload(MessageType.PROP_SYNC_REQUEST, encoded)

        assert decoded.lxmf_dest_hash == bytes.fromhex("abcdef0123456789abcdef0123456789")
        assert len(decoded.known_ids) == 1
        assert decoded.max_messages == 5

    def test_empty_known_ids(self):
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=bytes(16),
            known_ids=[],
        )

        encoded = _encode_payload(MessageType.PROP_SYNC_REQUEST, payload)
        decoded = _decode_payload(MessageType.PROP_SYNC_REQUEST, encoded)

        assert decoded.known_ids == []
        assert decoded.max_messages == 10  # default

    def test_canonical_encoding(self):
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=bytes(16),
            known_ids=[],
            max_messages=10,
        )

        encoded = _encode_payload(MessageType.PROP_SYNC_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "lxmf_dest_hash" in data
        assert "known_ids" in data
        assert "max_messages" in data


class TestPropSyncResponsePayload:
    """Test PropSyncResponsePayload serialization."""

    def test_encode_decode_roundtrip(self):
        payload = PropSyncResponsePayload(count=3)

        encoded = _encode_payload(MessageType.PROP_SYNC_RESPONSE, payload)
        decoded = _decode_payload(MessageType.PROP_SYNC_RESPONSE, encoded)

        assert decoded.count == 3
        assert decoded.error is None

    def test_with_error(self):
        payload = PropSyncResponsePayload(count=0, error="Propagation not enabled")

        encoded = _encode_payload(MessageType.PROP_SYNC_RESPONSE, payload)
        decoded = _decode_payload(MessageType.PROP_SYNC_RESPONSE, encoded)

        assert decoded.count == 0
        assert decoded.error == "Propagation not enabled"


class TestPropMsgDeliverPayload:
    """Test PropMsgDeliverPayload serialization."""

    def test_encode_decode_roundtrip(self):
        fake_lxmf = bytes(range(100))
        payload = PropMsgDeliverPayload(
            transient_id=bytes(32),
            raw_lxmf=fake_lxmf,
        )

        encoded = _encode_payload(MessageType.PROP_MSG_DELIVER, payload)
        decoded = _decode_payload(MessageType.PROP_MSG_DELIVER, encoded)

        assert decoded.transient_id == bytes(32)
        assert decoded.raw_lxmf == fake_lxmf

    def test_canonical_encoding(self):
        payload = PropMsgDeliverPayload(
            transient_id=b"\xaa" * 32,
            raw_lxmf=b"\xbb" * 50,
        )

        encoded = _encode_payload(MessageType.PROP_MSG_DELIVER, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "transient_id" in data
        assert "raw_lxmf" in data
        assert isinstance(data["transient_id"], bytes)
        assert isinstance(data["raw_lxmf"], bytes)


class TestPropSubmitRequestPayload:
    """Test PropSubmitRequestPayload serialization."""

    def test_encode_decode_roundtrip(self):
        fake_lxmf = bytes(range(200))
        payload = PropSubmitRequestPayload(raw_lxmf=fake_lxmf)

        encoded = _encode_payload(MessageType.PROP_SUBMIT_REQUEST, payload)
        decoded = _decode_payload(MessageType.PROP_SUBMIT_REQUEST, encoded)

        assert decoded.raw_lxmf == fake_lxmf


class TestPropSubmitResponsePayload:
    """Test PropSubmitResponsePayload serialization."""

    def test_accepted(self):
        payload = PropSubmitResponsePayload(
            accepted=True,
            transient_id=bytes(32),
        )

        encoded = _encode_payload(MessageType.PROP_SUBMIT_RESPONSE, payload)
        decoded = _decode_payload(MessageType.PROP_SUBMIT_RESPONSE, encoded)

        assert decoded.accepted is True
        assert decoded.transient_id == bytes(32)
        assert decoded.error is None

    def test_rejected(self):
        payload = PropSubmitResponsePayload(
            accepted=False,
            error="Propagation store full",
        )

        encoded = _encode_payload(MessageType.PROP_SUBMIT_RESPONSE, payload)
        decoded = _decode_payload(MessageType.PROP_SUBMIT_RESPONSE, encoded)

        assert decoded.accepted is False
        assert decoded.error == "Propagation store full"
