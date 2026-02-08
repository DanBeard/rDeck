"""Tests for ReticulumService using mock Reticulum layer.

These tests verify the companion server's message handling, trust workflows,
and service request/response flows without requiring actual Reticulum networking.
"""

import sys
import time
import pytest
import tempfile
from pathlib import Path
from unittest.mock import MagicMock, patch

from tests.mocks import (
    create_test_harness,
    MockLXMessage,
    MockTransport,
    MockLXMRouter,
)


@pytest.fixture
def temp_dir():
    """Create a temporary directory for test data."""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir)


@pytest.fixture
def harness():
    """Create and install a test harness.

    This must run before any imports of reticulum_service.
    """
    # Remove cached module so it reimports with mocks
    sys.modules.pop("companion_server.reticulum_service", None)

    h = create_test_harness()
    h.install()
    yield h
    h.uninstall()

    # Clean up cached module again
    sys.modules.pop("companion_server.reticulum_service", None)


@pytest.fixture
def config(temp_dir):
    """Create a test configuration."""
    from companion_server.config import Config

    cfg = Config(data_dir=temp_dir)
    cfg.server_name = "TestServer"
    cfg.enabled_services = ["ntp", "search"]
    return cfg


@pytest.fixture
def trust_manager(temp_dir):
    """Create a test trust manager."""
    from companion_server.trust_manager import TrustManager

    # TrustManager expects a directory, it will create trust.json inside
    return TrustManager(temp_dir)


@pytest.fixture
def service(harness, config, trust_manager):
    """Create a ReticulumService with mocks installed.

    IMPORTANT: harness must be listed first to ensure mocks are installed
    before we import ReticulumService.
    """
    # Import here after harness is set up
    from companion_server.reticulum_service import ReticulumService

    svc = ReticulumService(config, trust_manager)

    # We need to manually initialize since we don't want the background thread
    # Call the internal initialization directly for synchronous testing
    svc._initialize_reticulum()

    # Register the router with the harness so we can simulate messages
    if svc._lxmf_router:
        harness.register_router(svc._lxmf_router)

    yield svc

    svc.stop()


class TestServiceInitialization:
    """Tests for service initialization."""

    def test_creates_identity(self, service):
        """Service should create or load an identity."""
        assert service._identity is not None
        assert service._identity.hash is not None
        assert len(service._identity.hash) == 16

    def test_creates_lxmf_router(self, service):
        """Service should create an LXMF router."""
        assert service._lxmf_router is not None

    def test_creates_destination(self, service):
        """Service should have a destination for receiving messages."""
        assert service._lxmf_destination is not None
        assert service._lxmf_destination.hash is not None

    def test_announces_on_start(self, harness, service):
        """Service should announce itself on startup."""
        announces = harness.announces_sent
        assert len(announces) >= 1

    def test_identity_hash_property(self, service):
        """identity_hash property should return hex string."""
        assert service.identity_hash is not None
        assert isinstance(service.identity_hash, str)
        assert len(service.identity_hash) == 32  # 16 bytes as hex

    def test_destination_hash_property(self, service):
        """destination_hash property should return hex string."""
        assert service.destination_hash is not None
        assert isinstance(service.destination_hash, str)
        assert len(service.destination_hash) == 32


class TestAnnounceHandling:
    """Tests for incoming announce handling."""

    def test_receives_announce(self, harness, service):
        """Service should receive and store announces."""
        device_hash = b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
        device_name = "TestDevice"

        harness.simulate_announce(device_hash, device_name)

        announces = service.announces
        assert len(announces) == 1
        assert device_hash.hex() in announces

    def test_parses_display_name(self, harness, service):
        """Service should parse display name from app_data."""
        device_hash = b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
        device_name = "MyDevice"

        harness.simulate_announce(device_hash, device_name)

        announce = service.announces[device_hash.hex()]
        assert announce.display_name == device_name

    def test_announce_callback_invoked(self, harness, service):
        """Announce callbacks should be invoked."""
        received = []
        service.on_announce(lambda info: received.append(info))

        device_hash = b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
        harness.simulate_announce(device_hash, "Device")

        assert len(received) == 1
        assert received[0].destination_hash == device_hash

    def test_multiple_announces(self, harness, service):
        """Service should track multiple announces."""
        hash1 = b"\x01" * 16
        hash2 = b"\x02" * 16
        hash3 = b"\x03" * 16

        harness.simulate_announce(hash1, "Device1")
        harness.simulate_announce(hash2, "Device2")
        harness.simulate_announce(hash3, "Device3")

        assert len(service.announces) == 3

    def test_announce_updates_existing(self, harness, service):
        """Later announces from same device should update the entry."""
        device_hash = b"\x01" * 16

        harness.simulate_announce(device_hash, "OldName")
        harness.simulate_announce(device_hash, "NewName")

        assert len(service.announces) == 1
        assert service.announces[device_hash.hex()].display_name == "NewName"

    def test_parses_companion_server_from_prefix(self, harness, service):
        """Service should detect companion-server from [CS] display name prefix."""
        import msgpack

        device_hash = b"\x01" * 16
        # New format: [CS] prefix in name, None for stamp_cost
        app_data = msgpack.packb(["[CS] OtherServer".encode(), None])

        harness.simulate_announce(device_hash, "[CS] OtherServer", app_data=app_data)

        announce = service.announces[device_hash.hex()]
        assert announce.device_type == "companion-server"
        assert announce.display_name == "OtherServer"  # Prefix stripped
        assert announce.is_companion_server is True
        assert announce.is_known_type is True

    def test_parses_rdeck_from_prefix(self, harness, service):
        """Service should detect rdeck from [rD] display name prefix."""
        import msgpack

        device_hash = b"\x02" * 16
        # New format: [rD] prefix in name, None for stamp_cost
        app_data = msgpack.packb(["[rD] MyRdeck".encode(), None])

        harness.simulate_announce(device_hash, "[rD] MyRdeck", app_data=app_data)

        announce = service.announces[device_hash.hex()]
        assert announce.device_type == "rdeck"
        assert announce.display_name == "MyRdeck"  # Prefix stripped
        assert announce.is_rdeck is True
        assert announce.is_known_type is True

    def test_parses_legacy_metadata_format(self, harness, service):
        """Service should still parse legacy metadata dict for backwards compat."""
        import msgpack

        device_hash = b"\x04" * 16
        # Legacy format: metadata dict in second position (non-standard)
        metadata = {"type": "rdeck"}
        app_data = msgpack.packb(["LegacyDevice".encode(), metadata])

        harness.simulate_announce(device_hash, "LegacyDevice", app_data=app_data)

        announce = service.announces[device_hash.hex()]
        assert announce.device_type == "rdeck"
        assert announce.is_rdeck is True

    def test_unknown_device_has_no_type(self, harness, service):
        """Announces without metadata should have device_type=None."""
        import msgpack

        device_hash = b"\x03" * 16
        # rDeck format without metadata: [name_binary, null]
        app_data = msgpack.packb(["UnknownDevice".encode(), None])

        harness.simulate_announce(device_hash, "UnknownDevice", app_data=app_data)

        announce = service.announces[device_hash.hex()]
        assert announce.device_type is None
        assert announce.is_known_type is False
        assert announce.is_companion_server is False
        assert announce.is_rdeck is False


class TestTrustWorkflow:
    """Tests for trust offer/accept/revoke workflow."""

    def test_send_trust_offer(self, harness, service, trust_manager):
        """Sending a trust offer should send LXMF message and mark pending."""
        device_hash = b"\x01" * 16
        harness.simulate_announce(device_hash, "Device")

        service.send_trust_offer(device_hash.hex())

        # Should have sent a message
        messages = harness.sent_messages
        assert len(messages) == 1
        assert messages[0].destination_hash == device_hash

        # Should be marked pending in trust manager
        pending = trust_manager.get_pending_devices()
        assert len(pending) == 1
        assert pending[0].hash == device_hash.hex()

    def test_trust_offer_message_format(self, harness, service):
        """Trust offer message should have correct format."""
        from companion_server.protocol import MessageType

        device_hash = b"\x01" * 16
        harness.simulate_announce(device_hash, "Device")

        service.send_trust_offer(device_hash.hex())

        message = harness.sent_messages[0]
        fields = message.fields

        assert fields["msg_type"] == MessageType.TRUST_OFFER.value
        assert fields["service"] == "trust"
        assert "payload" in fields

    def test_receive_trust_accept(self, harness, service, trust_manager):
        """Receiving TRUST_ACCEPT should establish mutual trust."""
        from companion_server.protocol import (
            MessageType,
            TrustAcceptPayload,
            encode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        # First offer trust
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())
        harness.clear_sent()

        # Now simulate device accepting
        accept_msg = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="MyDevice"),
            request_id=12345,
        )
        fields = encode_service_fields(accept_msg)

        harness.simulate_message(device_hash, fields)

        # Should now be mutually trusted
        assert trust_manager.is_mutually_trusted(device_hash.hex())
        mutual = trust_manager.get_mutual_devices()
        assert len(mutual) == 1
        assert mutual[0].name == "MyDevice"

    def test_receive_trust_revoke(self, harness, service, trust_manager):
        """Receiving TRUST_REVOKE should remove trust."""
        from companion_server.protocol import (
            MessageType,
            TrustAcceptPayload,
            encode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        # Establish trust first
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())

        accept_msg = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="Device"),
            request_id=1,
        )
        harness.simulate_message(device_hash, encode_service_fields(accept_msg))
        assert trust_manager.is_mutually_trusted(device_hash.hex())

        # Now revoke
        revoke_msg = ServiceMessage(
            msg_type=MessageType.TRUST_REVOKE,
            service="trust",
            payload=None,
            request_id=2,
        )
        harness.simulate_message(device_hash, encode_service_fields(revoke_msg))

        # Should no longer be trusted
        assert not trust_manager.is_mutually_trusted(device_hash.hex())


class TestNTPService:
    """Tests for NTP request/response handling."""

    def test_ntp_request_requires_trust(self, harness, service, trust_manager):
        """NTP requests from untrusted devices should be rejected."""
        from companion_server.protocol import (
            MessageType,
            NTPRequestPayload,
            encode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        ntp_request = ServiceMessage(
            msg_type=MessageType.NTP_REQUEST,
            service="ntp",
            payload=NTPRequestPayload(client_timestamp=1234567890.0),
            request_id=100,
        )
        harness.simulate_message(device_hash, encode_service_fields(ntp_request))

        # Should not send response
        assert len(harness.sent_messages) == 0

    def test_ntp_request_from_trusted_device(self, harness, service, trust_manager):
        """NTP requests from trusted devices should get responses."""
        from companion_server.protocol import (
            MessageType,
            NTPRequestPayload,
            TrustAcceptPayload,
            encode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        # Establish trust
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())
        accept = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="Device"),
            request_id=1,
        )
        harness.simulate_message(device_hash, encode_service_fields(accept))
        harness.clear_sent()

        # Now send NTP request
        client_time = 1234567890.123
        ntp_request = ServiceMessage(
            msg_type=MessageType.NTP_REQUEST,
            service="ntp",
            payload=NTPRequestPayload(client_timestamp=client_time),
            request_id=100,
        )
        harness.simulate_message(device_hash, encode_service_fields(ntp_request))

        # Should send response
        messages = harness.sent_messages
        assert len(messages) == 1
        assert messages[0].destination_hash == device_hash

    def test_ntp_response_format(self, harness, service, trust_manager):
        """NTP response should have correct format."""
        from companion_server.protocol import (
            MessageType,
            NTPRequestPayload,
            TrustAcceptPayload,
            encode_service_fields,
            decode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        # Establish trust
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())
        accept = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="Device"),
            request_id=1,
        )
        harness.simulate_message(device_hash, encode_service_fields(accept))
        harness.clear_sent()

        # Send NTP request
        client_time = 1234567890.123
        ntp_request = ServiceMessage(
            msg_type=MessageType.NTP_REQUEST,
            service="ntp",
            payload=NTPRequestPayload(client_timestamp=client_time),
            request_id=100,
        )
        harness.simulate_message(device_hash, encode_service_fields(ntp_request))

        # Check response format
        message = harness.sent_messages[0]
        fields = message.fields

        assert fields["msg_type"] == MessageType.NTP_RESPONSE.value
        assert fields["service"] == "ntp"
        assert fields["request_id"] == 100

        # Decode and verify payload
        response = decode_service_fields(fields)
        assert response.payload.client_timestamp == client_time
        assert response.payload.server_timestamp > 0


class TestImplicitTrustUpgrade:
    """Tests for implicit trust upgrade when pending devices send requests."""

    def test_ntp_request_upgrades_pending_to_mutual(self, harness, service, trust_manager):
        """NTP request from pending device should upgrade trust to mutual."""
        from companion_server.protocol import (
            MessageType,
            NTPRequestPayload,
            encode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        # Send trust offer (device is now OFFERED/pending)
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())
        harness.clear_sent()

        # Verify device is pending, not mutual
        assert trust_manager.is_trust_pending(device_hash.hex())
        assert not trust_manager.is_mutually_trusted(device_hash.hex())

        # Device sends NTP request without explicit TRUST_ACCEPT
        ntp_request = ServiceMessage(
            msg_type=MessageType.NTP_REQUEST,
            service="ntp",
            payload=NTPRequestPayload(client_timestamp=12345),
            request_id=100,
        )
        harness.simulate_message(device_hash, encode_service_fields(ntp_request))

        # Should have upgraded to mutual trust
        assert trust_manager.is_mutually_trusted(device_hash.hex())
        assert not trust_manager.is_trust_pending(device_hash.hex())

        # Should have sent NTP response
        assert len(harness.sent_messages) == 1

    def test_search_request_upgrades_pending_to_mutual(self, harness, service, trust_manager):
        """Search request from pending device should upgrade trust to mutual."""
        from companion_server.protocol import (
            MessageType,
            SearchRequestPayload,
            SearchResponsePayload,
            SearchResult,
            encode_service_fields,
            ServiceMessage,
        )
        from unittest.mock import MagicMock

        device_hash = b"\x02" * 16

        # Send trust offer (device is now OFFERED/pending)
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())
        harness.clear_sent()

        # Verify device is pending
        assert trust_manager.is_trust_pending(device_hash.hex())

        # Mock search service
        service._search_service.handle_request = MagicMock(
            return_value=SearchResponsePayload(
                query="test", results=[], error=None
            )
        )

        # Device sends search request without explicit TRUST_ACCEPT
        search_request = ServiceMessage(
            msg_type=MessageType.SEARCH_REQUEST,
            service="search",
            payload=SearchRequestPayload(query="test", max_results=5),
            request_id=200,
        )
        harness.simulate_message(device_hash, encode_service_fields(search_request))

        # Should have upgraded to mutual trust
        assert trust_manager.is_mutually_trusted(device_hash.hex())

        # Should have sent search response
        assert len(harness.sent_messages) == 1


class TestSearchService:
    """Tests for search request/response handling."""

    def test_search_request_requires_trust(self, harness, service, trust_manager):
        """Search requests from untrusted devices should be rejected."""
        from companion_server.protocol import (
            MessageType,
            SearchRequestPayload,
            encode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        search_request = ServiceMessage(
            msg_type=MessageType.SEARCH_REQUEST,
            service="search",
            payload=SearchRequestPayload(query="test", max_results=5),
            request_id=200,
        )
        harness.simulate_message(device_hash, encode_service_fields(search_request))

        # Should not send response
        assert len(harness.sent_messages) == 0

    def test_search_request_from_trusted_device(self, harness, service, trust_manager):
        """Search requests from trusted devices should get responses."""
        from companion_server.protocol import (
            MessageType,
            SearchRequestPayload,
            SearchResponsePayload,
            SearchResult,
            TrustAcceptPayload,
            encode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        # Establish trust
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())
        accept = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="Device"),
            request_id=1,
        )
        harness.simulate_message(device_hash, encode_service_fields(accept))
        harness.clear_sent()

        # Mock the search service to avoid network calls
        service._search_service.handle_request = MagicMock(
            return_value=SearchResponsePayload(
                query="test",
                results=[SearchResult(title="Test", url="http://test.com", snippet="A test")],
                error=None,
            )
        )

        # Send search request
        search_request = ServiceMessage(
            msg_type=MessageType.SEARCH_REQUEST,
            service="search",
            payload=SearchRequestPayload(query="test", max_results=5),
            request_id=200,
        )
        harness.simulate_message(device_hash, encode_service_fields(search_request))

        # Should send response
        messages = harness.sent_messages
        assert len(messages) == 1
        assert messages[0].destination_hash == device_hash

    def test_search_response_format(self, harness, service, trust_manager):
        """Search response should have correct format."""
        from companion_server.protocol import (
            MessageType,
            SearchRequestPayload,
            SearchResponsePayload,
            SearchResult,
            TrustAcceptPayload,
            encode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\x01" * 16

        # Establish trust
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())
        accept = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="Device"),
            request_id=1,
        )
        harness.simulate_message(device_hash, encode_service_fields(accept))
        harness.clear_sent()

        # Mock search service with SearchResult objects
        mock_results = [
            SearchResult(title="Result 1", url="http://example.com/1", snippet="First"),
            SearchResult(title="Result 2", url="http://example.com/2", snippet="Second"),
        ]
        service._search_service.handle_request = MagicMock(
            return_value=SearchResponsePayload(
                query="python",
                results=mock_results,
                error=None,
            )
        )

        # Send search request
        search_request = ServiceMessage(
            msg_type=MessageType.SEARCH_REQUEST,
            service="search",
            payload=SearchRequestPayload(query="python", max_results=5),
            request_id=200,
        )
        harness.simulate_message(device_hash, encode_service_fields(search_request))

        # Check response format
        message = harness.sent_messages[0]
        fields = message.fields

        assert fields["msg_type"] == MessageType.SEARCH_RESPONSE.value
        assert fields["service"] == "search"
        assert fields["request_id"] == 200


class TestCallbacks:
    """Tests for callback invocation."""

    def test_log_callback(self, harness, service):
        """Log callbacks should be invoked for various events."""
        logs = []
        service.on_log(lambda msg: logs.append(msg))

        device_hash = b"\x01" * 16
        harness.simulate_announce(device_hash, "Device")

        # Should have logged the announce
        assert any("announce" in log.lower() for log in logs)

    def test_message_callback(self, harness, service):
        """Message callbacks should be invoked for incoming messages."""
        from companion_server.protocol import (
            MessageType,
            TrustAcceptPayload,
            encode_service_fields,
            ServiceMessage,
        )

        received = []
        service.on_message(lambda msg, hash: received.append((msg, hash)))

        device_hash = b"\x01" * 16

        # Send a message
        harness.simulate_announce(device_hash, "Device")
        service.send_trust_offer(device_hash.hex())

        accept = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="Device"),
            request_id=1,
        )
        harness.simulate_message(device_hash, encode_service_fields(accept))

        assert len(received) == 1
        assert received[0][1] == device_hash


class TestEdgeCases:
    """Tests for edge cases and error handling."""

    def test_malformed_announce_app_data(self, harness, service):
        """Service should handle malformed app_data gracefully."""
        device_hash = b"\x01" * 16

        # Send malformed app_data (not valid msgpack)
        harness.transport.simulate_announce(
            device_hash,
            "Device",
            app_data=b"\xff\xff\xff",  # Invalid msgpack
        )

        # Should still store announce with fallback name
        announces = service.announces
        assert len(announces) == 1

    def test_message_without_fields(self, harness, service):
        """Service should handle messages without service fields."""
        device_hash = b"\x01" * 16

        # Create message without msg_type field
        message = MockLXMessage(
            destination_hash=service._lxmf_destination.hash,
            source_hash=device_hash,
            content="Just a regular message",
            fields={},
        )
        service._lxmf_router.simulate_delivery(message)

        # Should not crash, just log
        # No assertions needed - test passes if no exception

    def test_trust_offer_to_unknown_device(self, harness, service, trust_manager):
        """Sending trust offer to device not in announces should still work."""
        device_hash = b"\x01" * 16

        # Don't simulate announce, just try to offer trust
        service.send_trust_offer(device_hash.hex())

        # Should still send and track
        assert len(harness.sent_messages) == 1
        assert len(trust_manager.get_pending_devices()) == 1


class TestFullWorkflow:
    """Integration tests for complete workflows."""

    def test_complete_trust_and_ntp_workflow(self, harness, service, trust_manager):
        """Test complete flow: announce -> trust -> NTP request/response."""
        from companion_server.protocol import (
            MessageType,
            NTPRequestPayload,
            TrustAcceptPayload,
            encode_service_fields,
            decode_service_fields,
            ServiceMessage,
        )

        device_hash = b"\xde\xad\xbe\xef" * 4

        # 1. Device announces
        harness.simulate_announce(device_hash, "rDeck-001")
        assert len(service.announces) == 1

        # 2. Server offers trust
        service.send_trust_offer(device_hash.hex())
        assert len(trust_manager.get_pending_devices()) == 1

        # 3. Device accepts
        accept = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="rDeck-001"),
            request_id=1,
        )
        harness.simulate_message(device_hash, encode_service_fields(accept))
        assert trust_manager.is_mutually_trusted(device_hash.hex())

        harness.clear_sent()

        # 4. Device requests NTP
        client_time = time.time()
        ntp_req = ServiceMessage(
            msg_type=MessageType.NTP_REQUEST,
            service="ntp",
            payload=NTPRequestPayload(client_timestamp=client_time),
            request_id=100,
        )
        harness.simulate_message(device_hash, encode_service_fields(ntp_req))

        # 5. Verify response
        assert len(harness.sent_messages) == 1
        response = decode_service_fields(harness.sent_messages[0].fields)
        assert response.msg_type == MessageType.NTP_RESPONSE
        assert response.payload.client_timestamp == client_time
        assert response.request_id == 100
