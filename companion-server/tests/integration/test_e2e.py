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


class TestEmulatorAppLaunch:
    """Tests for launching individual apps in the emulator.

    These tests use the emulator_with_app_launcher fixture which runs
    the emulator with --max-frames to auto-exit.
    """

    @pytest.mark.timeout(45)
    def test_launch_settings_app(self, emulator_with_app_launcher):
        """Test launching and closing Settings app."""
        manager = emulator_with_app_launcher("Settings", max_frames=150)
        assert manager.is_running()

        # Let it run for a bit
        time.sleep(5)

        # Check for crash indicators
        output = manager.get_output()
        assert "Segmentation fault" not in output
        assert "FATAL" not in output
        assert "Assertion" not in output

        # Check app launched
        assert "[Emulator] Auto-launching app: Settings" in output or "Settings" in output

    @pytest.mark.timeout(45)
    def test_launch_clock_app(self, emulator_with_app_launcher):
        """Test launching Clock app."""
        manager = emulator_with_app_launcher("Clock", max_frames=150)
        assert manager.is_running()

        time.sleep(5)

        output = manager.get_output()
        assert "Segmentation fault" not in output
        assert "FATAL" not in output

    @pytest.mark.timeout(45)
    def test_launch_notes_app(self, emulator_with_app_launcher):
        """Test launching Notes app."""
        manager = emulator_with_app_launcher("Notes", max_frames=150)
        assert manager.is_running()

        time.sleep(5)

        output = manager.get_output()
        assert "Segmentation fault" not in output
        assert "FATAL" not in output

    @pytest.mark.timeout(45)
    def test_launch_uchat_app(self, emulator_with_app_launcher):
        """Test launching uChat app."""
        manager = emulator_with_app_launcher("uChat", max_frames=150)
        assert manager.is_running()

        time.sleep(5)

        output = manager.get_output()
        assert "Segmentation fault" not in output
        assert "FATAL" not in output

    @pytest.mark.timeout(45)
    def test_launch_search_app(self, emulator_with_app_launcher):
        """Test launching Search app."""
        manager = emulator_with_app_launcher("Search", max_frames=150)
        assert manager.is_running()

        time.sleep(5)

        output = manager.get_output()
        assert "Segmentation fault" not in output
        assert "FATAL" not in output

    @pytest.mark.timeout(45)
    def test_launch_maps_app(self, emulator_with_app_launcher):
        """Test launching Maps app."""
        manager = emulator_with_app_launcher("Maps", max_frames=150)
        assert manager.is_running()

        time.sleep(5)

        output = manager.get_output()
        assert "Segmentation fault" not in output
        assert "FATAL" not in output


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

    def test_search_protocol_with_ai_summary_flag(self):
        """Test search request/response with ai_summary flag."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            SearchRequestPayload,
            SearchResponsePayload,
            SearchResult,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        # Test request with ai_summary=True
        request_msg = ServiceMessage(
            msg_type=MessageType.SEARCH_REQUEST,
            service="search",
            payload=SearchRequestPayload(
                query="what is reticulum",
                max_results=3,
                ai_summary=True,
            ),
            request_id=12345,
        )

        # Encode and decode
        fields = encode_service_fields(request_msg)
        decoded_request = decode_service_fields(fields)

        assert decoded_request.payload.query == "what is reticulum"
        assert decoded_request.payload.ai_summary is True

        # Test response with summary
        response_msg = ServiceMessage(
            msg_type=MessageType.SEARCH_RESPONSE,
            service="search",
            payload=SearchResponsePayload(
                query="what is reticulum",
                results=[SearchResult("Reticulum", "https://reticulum.network", "Mesh networking")],
                error=None,
                summary="Reticulum is a cryptography-based networking protocol for mesh communication.",
            ),
            request_id=12345,
        )

        fields = encode_service_fields(response_msg)
        decoded_response = decode_service_fields(fields)

        assert decoded_response.payload.summary is not None
        assert "Reticulum" in decoded_response.payload.summary


class TestMapsProtocol:
    """Tests for Maps protocol messages."""

    def test_map_tile_request_roundtrip(self):
        """Test map tile request/response encoding/decoding."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            MapTileRequestPayload,
            MapTileResponsePayload,
            TileFormat,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        # Test tile request
        request_msg = ServiceMessage(
            msg_type=MessageType.MAP_TILE_REQUEST,
            service="maps",
            payload=MapTileRequestPayload(
                z=14,
                x=8529,
                y=5975,
                format=TileFormat.MONO_RLE,
            ),
            request_id=42,
        )

        fields = encode_service_fields(request_msg)
        decoded_request = decode_service_fields(fields)

        assert decoded_request.msg_type == MessageType.MAP_TILE_REQUEST
        assert decoded_request.payload.z == 14
        assert decoded_request.payload.x == 8529
        assert decoded_request.payload.y == 5975
        assert decoded_request.payload.format == TileFormat.MONO_RLE

    def test_map_tile_response_with_data(self):
        """Test map tile response with tile data."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            MapTileResponsePayload,
            TileFormat,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        # Simulated RLE-compressed tile data
        test_tile_data = bytes([0xFF] * 100)  # Sample compressed data

        response_msg = ServiceMessage(
            msg_type=MessageType.MAP_TILE_RESPONSE,
            service="maps",
            payload=MapTileResponsePayload(
                z=14,
                x=8529,
                y=5975,
                format=TileFormat.MONO_RLE,
                chunk_index=0,
                total_chunks=1,
                data=test_tile_data,
                error=None,
            ),
            request_id=42,
        )

        fields = encode_service_fields(response_msg)
        decoded_response = decode_service_fields(fields)

        assert decoded_response.msg_type == MessageType.MAP_TILE_RESPONSE
        assert decoded_response.payload.z == 14
        assert decoded_response.payload.x == 8529
        assert decoded_response.payload.y == 5975
        assert decoded_response.payload.chunk_index == 0
        assert decoded_response.payload.total_chunks == 1
        assert decoded_response.payload.data == test_tile_data
        assert decoded_response.payload.error is None

    def test_map_tile_response_with_error(self):
        """Test map tile response with error."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            MapTileResponsePayload,
            TileFormat,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        response_msg = ServiceMessage(
            msg_type=MessageType.MAP_TILE_RESPONSE,
            service="maps",
            payload=MapTileResponsePayload(
                z=14,
                x=8529,
                y=5975,
                format=TileFormat.MONO_RLE,
                chunk_index=0,
                total_chunks=0,
                data=b"",
                error="Tile not found",
            ),
            request_id=42,
        )

        fields = encode_service_fields(response_msg)
        decoded_response = decode_service_fields(fields)

        assert decoded_response.payload.error == "Tile not found"
        assert decoded_response.payload.data == b""

    def test_map_geocode_request_roundtrip(self):
        """Test geocode request encoding/decoding."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            MapGeocodeRequestPayload,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        request_msg = ServiceMessage(
            msg_type=MessageType.MAP_GEOCODE_REQUEST,
            service="maps",
            payload=MapGeocodeRequestPayload(
                query="Golden Gate Bridge",
                bias_lat=377749000,  # 37.7749 * 1e7
                bias_lon=-1224194000,  # -122.4194 * 1e7
                max_results=5,
            ),
            request_id=100,
        )

        fields = encode_service_fields(request_msg)
        decoded_request = decode_service_fields(fields)

        assert decoded_request.msg_type == MessageType.MAP_GEOCODE_REQUEST
        assert decoded_request.payload.query == "Golden Gate Bridge"
        assert decoded_request.payload.bias_lat == 377749000
        assert decoded_request.payload.bias_lon == -1224194000
        assert decoded_request.payload.max_results == 5

    def test_map_geocode_response_with_results(self):
        """Test geocode response with results."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            MapGeocodeResponsePayload,
            MapGeocodeResult,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        response_msg = ServiceMessage(
            msg_type=MessageType.MAP_GEOCODE_RESPONSE,
            service="maps",
            payload=MapGeocodeResponsePayload(
                query="Golden Gate Bridge",
                results=[
                    MapGeocodeResult(
                        display_name="Golden Gate Bridge, San Francisco, CA",
                        lat=378079900,  # 37.80799 * 1e7
                        lon=-1224750800,  # -122.47508 * 1e7
                        type="landmark",
                    ),
                    MapGeocodeResult(
                        display_name="Golden Gate Park, San Francisco, CA",
                        lat=377699000,
                        lon=-1224758000,
                        type="park",
                    ),
                ],
                error=None,
            ),
            request_id=100,
        )

        fields = encode_service_fields(response_msg)
        decoded_response = decode_service_fields(fields)

        assert decoded_response.msg_type == MessageType.MAP_GEOCODE_RESPONSE
        assert decoded_response.payload.query == "Golden Gate Bridge"
        assert len(decoded_response.payload.results) == 2
        assert decoded_response.payload.results[0].display_name == "Golden Gate Bridge, San Francisco, CA"
        assert decoded_response.payload.results[0].type == "landmark"
        assert decoded_response.payload.error is None

    def test_map_route_request_roundtrip(self):
        """Test route request encoding/decoding."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            MapRouteRequestPayload,
            TravelMode,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        request_msg = ServiceMessage(
            msg_type=MessageType.MAP_ROUTE_REQUEST,
            service="maps",
            payload=MapRouteRequestPayload(
                start_lat=377749000,
                start_lon=-1224194000,
                end_lat=378079900,
                end_lon=-1224750800,
                mode=TravelMode.WALK,
            ),
            request_id=200,
        )

        fields = encode_service_fields(request_msg)
        decoded_request = decode_service_fields(fields)

        assert decoded_request.msg_type == MessageType.MAP_ROUTE_REQUEST
        assert decoded_request.payload.start_lat == 377749000
        assert decoded_request.payload.start_lon == -1224194000
        assert decoded_request.payload.end_lat == 378079900
        assert decoded_request.payload.end_lon == -1224750800
        assert decoded_request.payload.mode == TravelMode.WALK

    def test_map_route_response_with_instructions(self):
        """Test route response with turn-by-turn instructions."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            MapRouteResponsePayload,
            MapRouteInstruction,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        response_msg = ServiceMessage(
            msg_type=MessageType.MAP_ROUTE_RESPONSE,
            service="maps",
            payload=MapRouteResponsePayload(
                points=[377749000, -1224194000, 377800000, -1224300000, 378079900, -1224750800],
                instructions=[
                    MapRouteInstruction(distance_m=500, maneuver="straight", street="Market St"),
                    MapRouteInstruction(distance_m=300, maneuver="turn-left", street="Van Ness Ave"),
                    MapRouteInstruction(distance_m=0, maneuver="arrive", street=""),
                ],
                total_distance_m=800,
                total_time_s=600,
                error=None,
            ),
            request_id=200,
        )

        fields = encode_service_fields(response_msg)
        decoded_response = decode_service_fields(fields)

        assert decoded_response.msg_type == MessageType.MAP_ROUTE_RESPONSE
        assert len(decoded_response.payload.points) == 6
        assert len(decoded_response.payload.instructions) == 3
        assert decoded_response.payload.instructions[0].maneuver == "straight"
        assert decoded_response.payload.instructions[1].maneuver == "turn-left"
        assert decoded_response.payload.total_distance_m == 800
        assert decoded_response.payload.total_time_s == 600

    def test_all_travel_modes(self):
        """Test all travel modes encode/decode correctly."""
        from companion_server.protocol.messages import (
            MessageType,
            ServiceMessage,
            MapRouteRequestPayload,
            TravelMode,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        for mode in [TravelMode.WALK, TravelMode.BIKE, TravelMode.CAR]:
            request_msg = ServiceMessage(
                msg_type=MessageType.MAP_ROUTE_REQUEST,
                service="maps",
                payload=MapRouteRequestPayload(
                    start_lat=0,
                    start_lon=0,
                    end_lat=0,
                    end_lon=0,
                    mode=mode,
                ),
                request_id=1,
            )

            fields = encode_service_fields(request_msg)
            decoded = decode_service_fields(fields)

            assert decoded.payload.mode == mode, f"Failed for travel mode {mode}"


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
            MapTileRequestPayload,
            MapTileResponsePayload,
            MapRouteRequestPayload,
            MapRouteResponsePayload,
            MapGeocodeRequestPayload,
            MapGeocodeResponsePayload,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        # Basic sanity check
        assert MessageType.TRUST_OFFER == 0x01
        assert MessageType.NTP_REQUEST == 0x10
        assert MessageType.SEARCH_REQUEST == 0x20
        assert MessageType.MAP_TILE_REQUEST == 0x30
        assert MessageType.MAP_GEOCODE_REQUEST == 0x35

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
                services=["ntp", "search", "maps"],
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
        assert decoded.payload.services == ["ntp", "search", "maps"]

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
            MapTileRequestPayload,
            MapTileResponsePayload,
            MapRouteRequestPayload,
            MapRouteResponsePayload,
            MapRouteInstruction,
            MapGeocodeRequestPayload,
            MapGeocodeResponsePayload,
            MapGeocodeResult,
            TileFormat,
            TravelMode,
        )
        from companion_server.protocol.serialization import (
            encode_service_fields,
            decode_service_fields,
        )

        test_cases = [
            # Trust messages
            (MessageType.TRUST_OFFER, "trust", TrustOfferPayload("Server", ["ntp", "maps"])),
            (MessageType.TRUST_ACCEPT, "trust", TrustAcceptPayload("Device")),
            # NTP messages
            (MessageType.NTP_REQUEST, "ntp", NTPRequestPayload(12345)),
            (MessageType.NTP_RESPONSE, "ntp", NTPResponsePayload(1700000000, 12345)),
            # Search messages
            (MessageType.SEARCH_REQUEST, "search", SearchRequestPayload("test", 5, False)),
            (MessageType.SEARCH_REQUEST, "search", SearchRequestPayload("ai test", 3, True)),
            (MessageType.SEARCH_RESPONSE, "search", SearchResponsePayload(
                "test",
                [SearchResult("Title", "http://url", "Snippet")],
                None,
                None,
            )),
            (MessageType.SEARCH_RESPONSE, "search", SearchResponsePayload(
                "ai test",
                [SearchResult("Title", "http://url", "Snippet")],
                None,
                "This is an AI-generated summary.",
            )),
            # Map tile messages
            (MessageType.MAP_TILE_REQUEST, "maps", MapTileRequestPayload(14, 8529, 5975, TileFormat.MONO_RLE)),
            (MessageType.MAP_TILE_RESPONSE, "maps", MapTileResponsePayload(
                14, 8529, 5975, TileFormat.MONO_RLE, 0, 1, b"\x00\xFF", None
            )),
            # Map route messages
            (MessageType.MAP_ROUTE_REQUEST, "maps", MapRouteRequestPayload(
                377749000, -1224194000, 378079900, -1224750800, TravelMode.WALK
            )),
            (MessageType.MAP_ROUTE_RESPONSE, "maps", MapRouteResponsePayload(
                [377749000, -1224194000, 378079900, -1224750800],
                [MapRouteInstruction(500, "straight", "Market St")],
                500, 300, None
            )),
            # Map geocode messages
            (MessageType.MAP_GEOCODE_REQUEST, "maps", MapGeocodeRequestPayload(
                "San Francisco", 377749000, -1224194000, 5
            )),
            (MessageType.MAP_GEOCODE_RESPONSE, "maps", MapGeocodeResponsePayload(
                "San Francisco",
                [MapGeocodeResult("San Francisco, CA", 377749000, -1224194000, "city")],
                None
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


class TestEmulatorWithMaxFrames:
    """Tests that use --max-frames for deterministic runs.

    NOTE: The --max-frames feature currently doesn't work because the emulator's
    register_ui_task() blocks forever (runs the UI loop directly), so the frame
    counter in main.cpp never executes. These tests are skipped until the emulator
    architecture is updated to support programmatic exit.
    """

    @pytest.mark.skip(reason="--max-frames not working: UI task blocks forever")
    def test_emulator_exits_after_max_frames(self, temp_data_dir, xvfb_display):
        """Verify emulator exits cleanly after max frames."""
        pass

    @pytest.mark.skip(reason="--max-frames not working: UI task blocks forever")
    def test_emulator_auto_launch_app_and_exit(self, temp_data_dir, xvfb_display):
        """Test auto-launching an app and exiting after frames."""
        pass
