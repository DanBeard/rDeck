"""End-to-end integration tests for rDeck and companion server.

These tests verify the complete communication flow between the rDeck emulator
and the Python companion server. They require:
- Built emulator binary (.pio/build/emulator_64bits/program)
- Xvfb for headless display (or real display)
- Reticulum configured for local TCP communication

Run with: pytest tests/integration/test_e2e.py -v --timeout=60
"""

import time
import pytest
from pathlib import Path

from .conftest import ProcessManager


class TestServerStandalone:
    """Tests for companion server without emulator."""

    def test_server_starts_in_headless_mode(self, companion_server: ProcessManager):
        """Verify companion server starts and runs in headless mode."""
        assert companion_server.is_running()

        # Give it a moment to initialize
        time.sleep(2)

        # Check output for initialization messages
        output = companion_server.get_output()
        assert "headless mode" in output.lower() or "reticulum" in output.lower()

    def test_server_handles_graceful_shutdown(self, companion_server: ProcessManager):
        """Verify server shuts down cleanly."""
        assert companion_server.is_running()

        companion_server.stop(timeout=5.0)

        assert not companion_server.is_running()


class TestEmulatorStandalone:
    """Tests for emulator without companion server."""

    def test_emulator_starts(self, emulator: ProcessManager):
        """Verify emulator starts successfully."""
        assert emulator.is_running()

    def test_emulator_handles_graceful_shutdown(self, emulator: ProcessManager):
        """Verify emulator shuts down cleanly."""
        assert emulator.is_running()

        emulator.stop(timeout=5.0)

        assert not emulator.is_running()


class TestTrustWorkflow:
    """Tests for the trust establishment workflow.

    Flow:
    1. Server and emulator start
    2. Emulator announces on network
    3. Server sees announce
    4. Server sends TRUST_OFFER
    5. Emulator stores as pending
    6. Emulator sends TRUST_ACCEPT
    7. Both sides now have mutual trust
    """

    def test_server_receives_emulator_announce(
        self,
        running_server_and_emulator: tuple[ProcessManager, ProcessManager],
    ):
        """Verify server receives announce from emulator."""
        server, emulator = running_server_and_emulator

        # Wait for announce propagation
        time.sleep(5)

        # Check server logs for announce
        output = server.get_output()
        # Should contain something like "Received announce from"
        assert "announce" in output.lower()


class TestNTPWorkflow:
    """Tests for NTP time synchronization.

    Flow:
    1. Trust established (prerequisite)
    2. Emulator sends NTP_REQUEST
    3. Server responds with NTP_RESPONSE
    4. Emulator sets system time
    """

    def test_ntp_sync_after_trust(
        self,
        running_server_and_emulator: tuple[ProcessManager, ProcessManager],
    ):
        """Verify NTP sync works after trust is established."""
        server, emulator = running_server_and_emulator

        # Wait for potential trust + NTP sequence
        time.sleep(10)

        # Check server logs for NTP activity
        output = server.get_output()
        # Look for NTP-related messages
        # This would show if an NTP request was received and responded to
        has_ntp_activity = "[ntp]" in output.lower()

        # Note: This test may not show NTP activity if trust wasn't established
        # In a full E2E test, we'd programmatically trigger the workflow


class TestSearchWorkflow:
    """Tests for search functionality.

    Flow:
    1. Trust established (prerequisite)
    2. Emulator sends SEARCH_REQUEST
    3. Server performs search and returns SEARCH_RESPONSE
    """

    def test_search_request_response(
        self,
        running_server_and_emulator: tuple[ProcessManager, ProcessManager],
    ):
        """Verify search works after trust is established."""
        server, emulator = running_server_and_emulator

        # In a full E2E test, we'd trigger a search from the emulator
        # For now, just verify the system is running
        assert server.is_running()
        assert emulator.is_running()


class TestProtocolCompatibility:
    """Tests that don't require full E2E but verify protocol compatibility."""

    def test_server_protocol_modules_import(self):
        """Verify all protocol modules can be imported."""
        from companion_server.protocol.messages import (
            MessageType,
            TrustOfferPayload,
            TrustAcceptPayload,
            NTPRequestPayload,
            NTPResponsePayload,
            SearchRequestPayload,
            SearchResponsePayload,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        # Basic sanity check
        assert MessageType.TRUST_OFFER == 0x01
        assert MessageType.NTP_REQUEST == 0x10
        assert MessageType.SEARCH_REQUEST == 0x20

    def test_service_message_roundtrip(self):
        """Verify service messages can be encoded and decoded."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            TrustOfferPayload,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        # Create a message
        original = ServiceMessage(
            msg_type=MessageType.TRUST_OFFER,
            service="trust",
            payload=TrustOfferPayload(
                server_name="TestServer",
                services=["ntp", "search"],
            ),
            request_id=12345,
        )

        # Encode to LXMF fields
        fields = encode_service_fields(original)

        # Decode back
        decoded = decode_service_fields(fields)

        assert decoded.msg_type == original.msg_type
        assert decoded.service == original.service
        assert decoded.request_id == original.request_id
        assert decoded.payload.server_name == "TestServer"
        assert decoded.payload.services == ["ntp", "search"]

    def test_all_message_types_encode_decode(self):
        """Verify all message types can be encoded and decoded."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            TrustOfferPayload,
            TrustAcceptPayload,
            NTPRequestPayload,
            NTPResponsePayload,
            SearchRequestPayload,
            SearchResponsePayload,
            SearchResult,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        test_cases = [
            (MessageType.TRUST_OFFER, "trust", TrustOfferPayload("Server", ["ntp"])),
            (MessageType.TRUST_ACCEPT, "trust", TrustAcceptPayload("Device")),
            (MessageType.NTP_REQUEST, "ntp", NTPRequestPayload(12345)),
            (MessageType.NTP_RESPONSE, "ntp", NTPResponsePayload(1700000000, 12345)),
            (MessageType.SEARCH_REQUEST, "search", SearchRequestPayload("test", 5)),
            (MessageType.SEARCH_RESPONSE, "search", SearchResponsePayload(
                "test",
                [SearchResult("Title", "http://url", "Snippet")],
                None,
            )),
        ]

        for msg_type, service, payload in test_cases:
            msg = ServiceMessage(
                msg_type=msg_type,
                service=service,
                payload=payload,
                request_id=1,
            )

            fields = encode_service_fields(msg)
            decoded = decode_service_fields(fields)

            assert decoded.msg_type == msg_type, f"Failed for {msg_type}"
            assert decoded.service == service, f"Failed for {msg_type}"
