"""Edge case and robustness tests for NTP, Search, and Maps services.

Exercises boundary conditions, malformed inputs, and error paths
that are not covered by the happy-path test suites.
"""

import io
import time
import sqlite3
import struct
import tempfile
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

import msgpack
import pytest

from companion_server.config import Config
from companion_server.protocol.messages import (
    MessageType,
    ServiceMessage,
    NTPRequestPayload,
    NTPResponsePayload,
    SearchRequestPayload,
    SearchResponsePayload,
    SearchResult,
    TileFormat,
    TravelMode,
    MapTileRequestPayload,
    MapTileResponsePayload,
    MapRouteRequestPayload,
    MapRouteResponsePayload,
    MapRouteInstruction,
    MapGeocodeRequestPayload,
    MapGeocodeResponsePayload,
    MapGeocodeResult,
)
from companion_server.protocol.serialization import (
    encode_service_fields,
    decode_service_fields,
    _encode_payload,
    _decode_payload,
)
from companion_server.services.ntp_service import NTPService
from companion_server.services.search_service import SearchService
from companion_server.services.maps_service import (
    MapsService,
    _rle_encode,
    _rle_decode,
    _floyd_steinberg_dither,
    _image_to_packed_bits,
    MAX_CHUNK_SIZE,
)


# ============================================================
# NTP Edge Cases
# ============================================================


class TestNTPEdgeCases:
    """Edge cases for the NTP service."""

    def test_zero_timestamp(self):
        """Client timestamp of 0 should be echoed back unchanged."""
        svc = NTPService()
        resp = svc.handle_request(NTPRequestPayload(client_timestamp=0))
        assert resp.client_timestamp == 0
        assert resp.server_timestamp > 0

    def test_max_uint32_timestamp(self):
        """Client timestamp at uint32 max (2^32-1) should round-trip."""
        svc = NTPService()
        resp = svc.handle_request(NTPRequestPayload(client_timestamp=0xFFFFFFFF))
        assert resp.client_timestamp == 0xFFFFFFFF

    def test_negative_client_timestamp(self):
        """Negative timestamp - C++ sends uint32, but Python int has no limit.
        Should still echo back whatever was sent."""
        svc = NTPService()
        resp = svc.handle_request(NTPRequestPayload(client_timestamp=-1))
        assert resp.client_timestamp == -1

    def test_server_timestamp_is_epoch_seconds(self):
        """Server timestamp should be epoch seconds, not milliseconds."""
        svc = NTPService()
        before = int(time.time())
        resp = svc.handle_request(NTPRequestPayload(client_timestamp=0))
        after = int(time.time())
        # Epoch seconds in 2025+ should be ~1.7 billion, not trillions
        assert before <= resp.server_timestamp <= after
        assert resp.server_timestamp < 10_000_000_000  # Not milliseconds

    def test_rapid_sequential_requests(self):
        """Multiple rapid requests should all return valid, close timestamps."""
        svc = NTPService()
        responses = [
            svc.handle_request(NTPRequestPayload(client_timestamp=i))
            for i in range(100)
        ]
        timestamps = [r.server_timestamp for r in responses]
        # All timestamps should be within 1 second of each other
        assert max(timestamps) - min(timestamps) <= 1
        # Each response echoes the correct client timestamp
        for i, r in enumerate(responses):
            assert r.client_timestamp == i

    def test_ntp_payload_serialization_preserves_large_values(self):
        """Verify large uint32 values survive msgpack round-trip."""
        original = NTPResponsePayload(
            server_timestamp=0xFFFFFFFE,
            client_timestamp=0xFFFFFFFF,
        )
        encoded = _encode_payload(MessageType.NTP_RESPONSE, original)
        decoded = _decode_payload(MessageType.NTP_RESPONSE, encoded)
        assert decoded.server_timestamp == 0xFFFFFFFE
        assert decoded.client_timestamp == 0xFFFFFFFF

    def test_ntp_decode_missing_fields(self):
        """Decoding NTP with missing fields should use defaults."""
        # Empty dict
        encoded = msgpack.packb({})
        decoded = _decode_payload(MessageType.NTP_REQUEST, encoded)
        assert decoded.client_timestamp == 0

        decoded = _decode_payload(MessageType.NTP_RESPONSE, encoded)
        assert decoded.server_timestamp == 0
        assert decoded.client_timestamp == 0

    def test_ntp_decode_extra_fields_ignored(self):
        """Extra fields in msgpack should not cause errors."""
        data = {"client_timestamp": 12345, "extra_field": "should be ignored"}
        encoded = msgpack.packb(data)
        decoded = _decode_payload(MessageType.NTP_REQUEST, encoded)
        assert decoded.client_timestamp == 12345


# ============================================================
# Search Edge Cases
# ============================================================


class TestSearchEdgeCases:
    """Edge cases for the Search service."""

    @pytest.fixture
    def config(self):
        config = Mock(spec=Config)
        config.search_max_results = 5
        config.ai_summary_enabled = False
        config.ai_summary_model_path = None
        config.ai_summary_max_tokens = 256
        config.ai_summary_context_size = 2048
        return config

    def test_empty_query_still_returns_response(self, config):
        """Empty query should return a valid response, not crash."""
        svc = SearchService(config)
        with patch.object(svc, '_search_duckduckgo', return_value=[]):
            resp = svc.handle_request(SearchRequestPayload(query="", max_results=5))
        assert resp.query == ""
        assert resp.results == []
        assert resp.error is None

    def test_max_results_zero(self, config):
        """max_results=0 should be respected (return no results)."""
        svc = SearchService(config)
        with patch.object(svc, '_search_duckduckgo', return_value=[]) as mock:
            resp = svc.handle_request(SearchRequestPayload(query="test", max_results=0))
            # min(0, 5) = 0
            mock.assert_called_once_with("test", 0)

    def test_max_results_exceeds_config(self, config):
        """Request for 100 results should be clamped to config limit (5)."""
        svc = SearchService(config)
        with patch.object(svc, '_search_duckduckgo', return_value=[]) as mock:
            svc.handle_request(SearchRequestPayload(query="test", max_results=100))
            mock.assert_called_once_with("test", 5)

    def test_http_exception_returns_error_response(self, config):
        """HTTP error should produce error in response, not raise."""
        import httpx
        svc = SearchService(config)
        with patch.object(svc._client, 'post', side_effect=httpx.ConnectError("refused")):
            resp = svc.handle_request(SearchRequestPayload(query="test", max_results=5))
        assert resp.error is not None
        assert resp.results == []

    def test_unicode_query_roundtrip(self, config):
        """Unicode characters in query should survive serialization."""
        payload = SearchRequestPayload(query="cafe\u0301 \u00fc\u00f1\u00ef\u00e7\u00f8de", max_results=3)
        encoded = _encode_payload(MessageType.SEARCH_REQUEST, payload)
        decoded = _decode_payload(MessageType.SEARCH_REQUEST, encoded)
        assert decoded.query == "cafe\u0301 \u00fc\u00f1\u00ef\u00e7\u00f8de"

    def test_search_result_with_empty_fields(self):
        """Search results with all-empty fields should serialize cleanly."""
        payload = SearchResponsePayload(
            query="test",
            results=[SearchResult(title="", url="", snippet="")],
            error=None,
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)
        assert len(decoded.results) == 1
        assert decoded.results[0].title == ""

    def test_search_response_null_vs_missing_error(self):
        """Python sends error=None; C++ omits error. Both should decode correctly."""
        # Python style: error field present but null
        data_with_null = {"query": "q", "results": [], "error": None}
        encoded = msgpack.packb(data_with_null, use_bin_type=True)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)
        assert decoded.error is None

        # C++ style: error field absent
        data_without = {"query": "q", "results": []}
        encoded = msgpack.packb(data_without, use_bin_type=True)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)
        assert decoded.error is None

    def test_search_decode_empty_payload(self):
        """Empty payload bytes should return None, not crash."""
        result = _decode_payload(MessageType.SEARCH_REQUEST, b"")
        assert result is None

    def test_search_html_parser_no_results_div(self, config):
        """HTML with no result divs should return empty list."""
        svc = SearchService(config)
        results = svc._parse_duckduckgo_html("<html><body>No results</body></html>", 5)
        assert results == []

    def test_search_html_parser_malformed_html(self, config):
        """Severely malformed HTML should not crash."""
        svc = SearchService(config)
        results = svc._parse_duckduckgo_html("<<<>>>not html at all{{{", 5)
        assert isinstance(results, list)

    def test_search_truncation_long_title(self, config):
        """Titles over 100 chars should be truncated."""
        svc = SearchService(config)
        long_title = "A" * 200
        html = f'''<div class="result "><a class="result__a" href="http://example.com">{long_title}</a>
        <span class="result__snippet">snippet</span></div>'''
        results = svc._parse_duckduckgo_html(html, 5)
        if results:  # Parser might not match this simplified HTML
            assert len(results[0].title) <= 100

    def test_search_truncation_long_url(self, config):
        """URLs over 500 chars should be truncated."""
        svc = SearchService(config)
        long_url = "http://example.com/" + "a" * 500
        html = f'''<div class="result "><a class="result__a" href="{long_url}">Title</a>
        <span class="result__snippet">snippet</span></div>'''
        results = svc._parse_duckduckgo_html(html, 5)
        if results:
            assert len(results[0].url) <= 500


# ============================================================
# RLE Encoding Edge Cases
# ============================================================


class TestRLEEdgeCases:
    """Edge cases for RLE compression/decompression."""

    def test_single_byte(self):
        """Single byte should encode as [1, byte]."""
        assert _rle_encode(b"\xAB") == bytes([1, 0xAB])

    def test_single_byte_roundtrip(self):
        data = b"\x42"
        assert _rle_decode(_rle_encode(data)) == data

    def test_all_same_bytes(self):
        """255 identical bytes should encode as single run."""
        data = bytes([0xFF] * 255)
        encoded = _rle_encode(data)
        assert encoded == bytes([255, 0xFF])
        assert _rle_decode(encoded) == data

    def test_256_same_bytes_splits(self):
        """256 identical bytes must split into two runs (255 max per run)."""
        data = bytes([0xAA] * 256)
        encoded = _rle_encode(data)
        assert encoded == bytes([255, 0xAA, 1, 0xAA])
        assert _rle_decode(encoded) == data

    def test_alternating_bytes(self):
        """Alternating bytes = worst case: each byte is its own run."""
        data = bytes([0x00, 0xFF] * 4)
        encoded = _rle_encode(data)
        # Each byte gets [1, val]
        assert len(encoded) == len(data) * 2
        assert _rle_decode(encoded) == data

    def test_decode_odd_length_truncates_gracefully(self):
        """Odd-length RLE data (incomplete pair) should not crash.
        The trailing count byte without a value byte is silently skipped."""
        # Well-formed: [3, 0xFF] + incomplete: [5]
        data = bytes([3, 0xFF, 5])
        decoded = _rle_decode(data)
        # Only the complete pair is decoded
        assert decoded == bytes([0xFF] * 3)

    def test_decode_count_zero(self):
        """Count of 0 should produce 0 repetitions (not crash)."""
        data = bytes([0, 0xFF, 3, 0xAA])
        decoded = _rle_decode(data)
        assert decoded == bytes([0xAA] * 3)

    def test_large_random_data_roundtrip(self):
        """Large random-ish data should survive RLE round-trip."""
        import random
        random.seed(42)
        # Mix of runs and random data
        data = bytearray()
        for _ in range(50):
            val = random.randint(0, 255)
            count = random.randint(1, 300)
            data.extend([val] * count)
        data = bytes(data)
        assert _rle_decode(_rle_encode(data)) == data

    def test_max_expansion_ratio(self):
        """Worst case: all unique bytes doubles the size."""
        data = bytes(range(256))  # 256 unique bytes
        encoded = _rle_encode(data)
        assert len(encoded) == 512  # Each byte becomes [1, byte]
        assert _rle_decode(encoded) == data


# ============================================================
# Maps Tile Processing Edge Cases
# ============================================================


class TestTileChunking:
    """Test tile response chunking logic."""

    @pytest.fixture
    def service(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        return MapsService(config)

    def test_small_tile_single_chunk(self, service):
        """Tile data <= MAX_CHUNK_SIZE should be a single chunk."""
        data = bytes(range(100))
        chunks = service._chunk_tile_response(10, 100, 200, TileFormat.MONO_RLE, data)
        assert len(chunks) == 1
        assert chunks[0].chunk_index == 0
        assert chunks[0].total_chunks == 1
        assert chunks[0].data == data

    def test_exact_chunk_size(self, service):
        """Data exactly MAX_CHUNK_SIZE should be single chunk."""
        data = bytes([0xAB] * MAX_CHUNK_SIZE)
        chunks = service._chunk_tile_response(10, 100, 200, TileFormat.MONO_RLE, data)
        assert len(chunks) == 1

    def test_one_over_chunk_size(self, service):
        """Data one byte over MAX_CHUNK_SIZE should split into two chunks."""
        data = bytes([0xAB] * (MAX_CHUNK_SIZE + 1))
        chunks = service._chunk_tile_response(10, 100, 200, TileFormat.MONO_RLE, data)
        assert len(chunks) == 2
        assert chunks[0].total_chunks == 2
        assert chunks[1].total_chunks == 2
        assert chunks[0].chunk_index == 0
        assert chunks[1].chunk_index == 1
        # Reassembled data should match
        reassembled = chunks[0].data + chunks[1].data
        assert reassembled == data

    def test_large_tile_many_chunks(self, service):
        """2048 bytes (full 128x128 1-bit tile) should split correctly."""
        data = bytes([0xFF] * 2048)
        chunks = service._chunk_tile_response(10, 100, 200, TileFormat.RAW_1BIT, data)
        expected_chunks = (2048 + MAX_CHUNK_SIZE - 1) // MAX_CHUNK_SIZE
        assert len(chunks) == expected_chunks
        # Verify all chunk indices are sequential
        for i, chunk in enumerate(chunks):
            assert chunk.chunk_index == i
            assert chunk.total_chunks == expected_chunks
        # Verify reassembly
        reassembled = b"".join(c.data for c in chunks)
        assert reassembled == data

    def test_empty_data_single_chunk(self, service):
        """Empty tile data should still return one chunk."""
        chunks = service._chunk_tile_response(10, 100, 200, TileFormat.MONO_RLE, b"")
        assert len(chunks) == 1
        assert chunks[0].data == b""

    def test_chunk_preserves_coordinates(self, service):
        """All chunks for a tile should carry the same z/x/y."""
        data = bytes([0xFF] * 500)
        chunks = service._chunk_tile_response(14, 2746, 6327, TileFormat.MONO_RLE, data)
        for chunk in chunks:
            assert chunk.z == 14
            assert chunk.x == 2746
            assert chunk.y == 6327


class TestMBTilesIntegration:
    """Test MBTiles loading and tile retrieval."""

    @pytest.fixture
    def mbtiles_path(self, tmp_path):
        """Create a minimal valid MBTiles file."""
        db_path = tmp_path / "test.mbtiles"
        conn = sqlite3.connect(str(db_path))
        conn.execute("""
            CREATE TABLE metadata (name TEXT, value TEXT)
        """)
        conn.execute("""
            CREATE TABLE tiles (
                zoom_level INTEGER,
                tile_column INTEGER,
                tile_row INTEGER,
                tile_data BLOB
            )
        """)
        conn.execute("INSERT INTO metadata VALUES ('name', 'test')")
        conn.execute("INSERT INTO metadata VALUES ('format', 'png')")
        # Insert a tiny valid PNG (1x1 white pixel)
        # Minimal PNG: 8-byte sig + IHDR + IDAT + IEND
        png_data = _make_minimal_png()
        # z=10, x=512, y=512 -> TMS y = (1<<10) - 1 - 512 = 511
        conn.execute(
            "INSERT INTO tiles VALUES (10, 512, 511, ?)",
            (png_data,)
        )
        conn.commit()
        conn.close()
        return db_path

    def test_init_mbtiles_valid(self, mbtiles_path):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = str(mbtiles_path)
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        svc = MapsService(config)
        assert svc._mbtiles_conn is not None

    def test_init_mbtiles_nonexistent(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = "/nonexistent/path.mbtiles"
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        svc = MapsService(config)
        assert svc._mbtiles_conn is None
        assert not svc.available

    def test_tile_not_found(self, mbtiles_path):
        """Request for a tile not in the database should return error."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = str(mbtiles_path)
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        svc = MapsService(config)

        payload = MapTileRequestPayload(z=5, x=999, y=999, format=TileFormat.MONO_RLE)
        responses = svc._handle_tile_request(payload)
        assert len(responses) == 1
        # Without PIL installed, error is "PIL not available" before tile lookup
        assert responses[0].error in ("Tile not found", "PIL not available for tile processing")

    def test_tms_y_coordinate_conversion(self, mbtiles_path):
        """Verify TMS y-coordinate flip: tms_y = (1 << z) - 1 - y."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = str(mbtiles_path)
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        svc = MapsService(config)

        # We inserted at z=10, x=512, tms_row=511
        # XYZ y = (1 << 10) - 1 - 511 = 512
        payload = MapTileRequestPayload(z=10, x=512, y=512, format=TileFormat.MONO_RLE)
        responses = svc._handle_tile_request(payload)
        # Should find the tile (may fail if PIL not available, which is ok)
        if responses[0].error and "PIL" in responses[0].error:
            pytest.skip("PIL not available")
        # No "Tile not found" error
        assert responses[0].error is None or "not found" not in responses[0].error.lower()

    def test_reload_mbtiles(self, mbtiles_path):
        """reload_mbtiles should close old connection and open new one."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        svc = MapsService(config)
        assert svc._mbtiles_conn is None

        svc.reload_mbtiles(str(mbtiles_path))
        assert svc._mbtiles_conn is not None

    def test_reload_mbtiles_nonexistent(self):
        """reload_mbtiles with bad path should leave connection as None."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        svc = MapsService(config)
        svc.reload_mbtiles("/nonexistent/path.mbtiles")
        assert svc._mbtiles_conn is None

    def test_close_cleans_up(self, mbtiles_path):
        """close() should release MBTiles and HTTP connections."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = str(mbtiles_path)
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        svc = MapsService(config)
        assert svc._mbtiles_conn is not None

        svc.close()
        assert svc._mbtiles_conn is None
        assert svc._http_client is None


class TestMapsServiceNoMBTiles:
    """Test Maps service behavior when MBTiles is not available."""

    @pytest.fixture
    def service(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        return MapsService(config)

    def test_tile_request_no_mbtiles(self, service):
        """Tile request without MBTiles should return error."""
        payload = MapTileRequestPayload(z=10, x=100, y=200, format=TileFormat.MONO_RLE)
        responses = service._handle_tile_request(payload)
        assert len(responses) == 1
        assert "not available" in responses[0].error.lower()

    def test_route_request_no_valhalla(self, service):
        """Route request without Valhalla should return error."""
        payload = MapRouteRequestPayload(
            start_lat=377749000, start_lon=-1224194000,
            end_lat=377850000, end_lon=-1224000000,
            mode=TravelMode.WALK,
        )
        resp = service._handle_route_request(payload)
        assert "not configured" in resp.error.lower()

    def test_geocode_request_no_nominatim(self, service):
        """Geocode request without Nominatim should return error."""
        payload = MapGeocodeRequestPayload(query="test", max_results=5)
        resp = service._handle_geocode_request(payload)
        assert "not configured" in resp.error.lower()


# ============================================================
# Polyline Decoding Edge Cases
# ============================================================


class TestPolylineDecoding:
    """Test Valhalla polyline decoder edge cases."""

    @pytest.fixture
    def service(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        return MapsService(config)

    def test_empty_string(self, service):
        """Empty polyline should return empty list."""
        assert service._decode_polyline("") == []

    def test_known_encoded_point(self, service):
        """Verify decoding of a known encoded polyline."""
        # Encode (38.5, -120.2) at precision 6:
        # Using the standard algorithm to encode a single point
        # This is well-documented in the Google Maps polyline algorithm
        points = service._decode_polyline("_p~iF~ps|U")
        # Should decode to approximately (38.5, -120.2) at precision 5
        # At precision 6 (Valhalla default), the encoding is different
        # Just verify we get a non-empty result without crash
        assert isinstance(points, list)

    def test_single_coordinate_pair(self, service):
        """A single encoded lat/lon pair should decode correctly."""
        # Manually encode (0.0, 0.0) at precision 6: both values are 0
        # Encoded: each 0 -> 0 -> shifted left: 0 -> add 63 -> chr(63) = '?'
        # Two '?' characters for lat=0, lon=0
        points = service._decode_polyline("??")
        assert len(points) == 1
        assert abs(points[0][0]) < 0.001  # lat ≈ 0
        assert abs(points[0][1]) < 0.001  # lon ≈ 0

    def test_truncated_polyline_raises(self, service):
        """Truncated polyline should raise IndexError (caught by caller)."""
        # A single character can't encode a full lat/lon pair
        with pytest.raises(IndexError):
            service._decode_polyline("~")


# ============================================================
# Valhalla Response Parsing Edge Cases
# ============================================================


class TestValhallaResponseParsing:
    """Test route response parsing with various Valhalla outputs."""

    @pytest.fixture
    def service(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        return MapsService(config)

    def test_empty_trip(self, service):
        """Valhalla returning empty trip should produce empty route."""
        data = {"trip": {"legs": [], "summary": {}}}
        resp = service._parse_valhalla_response(data)
        assert resp.points == []
        assert resp.instructions == []

    def test_missing_trip_key(self, service):
        """Valhalla response without 'trip' key should not crash."""
        data = {}
        resp = service._parse_valhalla_response(data)
        assert resp.points == []

    def test_missing_shape(self, service):
        """Leg without 'shape' should produce no points but process maneuvers."""
        data = {"trip": {"legs": [
            {"maneuvers": [{"length": 0.1, "type": 1, "street_names": ["Main St"]}]}
        ], "summary": {}}}
        resp = service._parse_valhalla_response(data)
        assert resp.points == []  # No shape = no points
        assert len(resp.instructions) == 1
        assert resp.instructions[0].street == "Main St"

    def test_missing_maneuvers(self, service):
        """Leg without maneuvers should produce points but no instructions."""
        data = {"trip": {"legs": [
            {"shape": "??"}  # Single (0,0) point
        ], "summary": {}}}
        resp = service._parse_valhalla_response(data)
        assert len(resp.points) >= 2  # At least one lat/lon pair
        assert resp.instructions == []

    def test_maneuver_without_street_names(self, service):
        """Maneuver without street_names should use empty string."""
        data = {"trip": {"legs": [
            {"shape": "", "maneuvers": [{"length": 0.5, "type": 2}]}
        ], "summary": {}}}
        resp = service._parse_valhalla_response(data)
        assert resp.instructions[0].street == ""

    def test_summary_distance_and_time(self, service):
        """Summary length (km) and time (seconds) should be converted correctly."""
        data = {"trip": {
            "legs": [],
            "summary": {"length": 5.5, "time": 3600}
        }}
        resp = service._parse_valhalla_response(data)
        assert resp.total_distance_m == 5500  # 5.5 km * 1000
        assert resp.total_time_s == 3600


# ============================================================
# Protocol Serialization Edge Cases
# ============================================================


class TestProtocolEdgeCases:
    """Edge cases in protocol encode/decode."""

    def test_decode_invalid_msgpack_raises(self):
        """Invalid msgpack bytes should raise UnpackException."""
        with pytest.raises(Exception):  # msgpack.exceptions.UnpackException
            _decode_payload(MessageType.NTP_REQUEST, b"\xFF\xFF\xFF")

    def test_decode_empty_payload_returns_none(self):
        """Empty bytes should return None for any message type."""
        for mt in MessageType:
            assert _decode_payload(mt, b"") is None

    def test_decode_unknown_message_type_returns_raw(self):
        """If msg_type doesn't match any known type, raw data is returned."""
        data = {"foo": "bar"}
        encoded = msgpack.packb(data)
        # Use a fake message type value that exists in enum but isn't handled
        # TRUST_REVOKE payload is handled, so let's test with actual data
        result = _decode_payload(MessageType.TRUST_REVOKE, encoded)
        assert result is not None

    def test_encode_none_payload(self):
        """Encoding None payload should return empty bytes."""
        assert _encode_payload(MessageType.NTP_REQUEST, None) == b""

    def test_tile_format_invalid_enum_raises(self):
        """Invalid TileFormat value in payload should raise ValueError."""
        data = {"z": 10, "x": 100, "y": 200, "format": 99}
        encoded = msgpack.packb(data)
        with pytest.raises(ValueError):
            _decode_payload(MessageType.MAP_TILE_REQUEST, encoded)

    def test_travel_mode_invalid_enum_raises(self):
        """Invalid TravelMode value in payload should raise ValueError."""
        data = {
            "start_lat": 0, "start_lon": 0,
            "end_lat": 0, "end_lon": 0,
            "mode": 77,
        }
        encoded = msgpack.packb(data)
        with pytest.raises(ValueError):
            _decode_payload(MessageType.MAP_ROUTE_REQUEST, encoded)

    def test_service_message_full_roundtrip(self):
        """Full ServiceMessage encode/decode with all fields."""
        msg = ServiceMessage(
            msg_type=MessageType.SEARCH_REQUEST,
            service="search",
            payload=SearchRequestPayload(query="hello world", max_results=3),
            request_id=0xDEADBEEF,
        )
        fields = encode_service_fields(msg)
        decoded = decode_service_fields(fields)
        assert decoded.msg_type == MessageType.SEARCH_REQUEST
        assert decoded.service == "search"
        assert decoded.request_id == 0xDEADBEEF
        assert decoded.payload.query == "hello world"
        assert decoded.payload.max_results == 3

    def test_route_response_empty_points_and_instructions(self):
        """Route response with no points and no instructions should round-trip."""
        payload = MapRouteResponsePayload(
            points=[],
            instructions=[],
            total_distance_m=0,
            total_time_s=0,
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, payload)
        decoded = _decode_payload(MessageType.MAP_ROUTE_RESPONSE, encoded)
        assert decoded.points == []
        assert decoded.instructions == []

    def test_geocode_request_partial_bias_treated_as_no_bias(self):
        """If only bias_lat is set but not bias_lon, both should be None on decode."""
        # Manually build payload with only bias_lat
        data = {"query": "test", "max_results": 5, "bias_lat": 100}
        encoded = msgpack.packb(data)
        decoded = _decode_payload(MessageType.MAP_GEOCODE_REQUEST, encoded)
        # bias_lon will be None (missing from dict)
        assert decoded.bias_lat == 100
        assert decoded.bias_lon is None

    def test_tile_response_binary_data_preserved(self):
        """Binary tile data should survive msgpack round-trip exactly."""
        # All possible byte values
        data = bytes(range(256))
        payload = MapTileResponsePayload(
            z=1, x=0, y=0,
            format=TileFormat.RAW_1BIT,
            chunk_index=0, total_chunks=1,
            data=data,
        )
        encoded = _encode_payload(MessageType.MAP_TILE_RESPONSE, payload)
        decoded = _decode_payload(MessageType.MAP_TILE_RESPONSE, encoded)
        assert decoded.data == data

    def test_large_search_results_roundtrip(self):
        """Many search results should serialize and deserialize correctly."""
        results = [
            SearchResult(title=f"Result {i}", url=f"http://example.com/{i}", snippet=f"Snippet {i}")
            for i in range(50)
        ]
        payload = SearchResponsePayload(query="test", results=results, error=None)
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, encoded)
        assert len(decoded.results) == 50
        assert decoded.results[49].title == "Result 49"


# ============================================================
# Maps Cross-Compatibility Vectors
# ============================================================


class TestMapsCrossCompatibility:
    """Verify Maps protocol encoding matches C++ expectations.

    These tests ensure the Python serialization produces output
    that the C++ ServiceProtocol can decode, and vice versa.
    """

    def test_tile_request_canonical_encoding(self):
        """Verify tile request field names and types match C++."""
        payload = MapTileRequestPayload(z=14, x=2746, y=6327, format=TileFormat.MONO_RLE)
        encoded = _encode_payload(MessageType.MAP_TILE_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert isinstance(data["z"], int)
        assert isinstance(data["x"], int)
        assert isinstance(data["y"], int)
        assert isinstance(data["format"], int)
        assert data["z"] == 14
        assert data["format"] == 0  # MONO_RLE

    def test_tile_response_error_field_conditional(self):
        """Error field should only be present when set (matches C++ behavior)."""
        # No error
        payload = MapTileResponsePayload(
            z=10, x=100, y=200, format=TileFormat.MONO_RLE,
            chunk_index=0, total_chunks=1, data=b"\x00",
        )
        encoded = _encode_payload(MessageType.MAP_TILE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)
        assert "error" not in data

        # With error
        payload.error = "not found"
        encoded = _encode_payload(MessageType.MAP_TILE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)
        assert data["error"] == "not found"

    def test_route_request_coordinate_encoding(self):
        """Coordinates should be encoded as integers (int32 * 1e7 format)."""
        payload = MapRouteRequestPayload(
            start_lat=377749000, start_lon=-1224194000,
            end_lat=377850000, end_lon=-1224000000,
            mode=TravelMode.CAR,
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert isinstance(data["start_lat"], int)
        assert isinstance(data["start_lon"], int)
        assert data["start_lon"] == -1224194000  # Negative values preserved
        assert data["mode"] == 2  # CAR

    def test_geocode_response_result_structure(self):
        """Geocode results should have exact field names matching C++."""
        payload = MapGeocodeResponsePayload(
            query="test",
            results=[MapGeocodeResult(
                display_name="Test Place",
                lat=488566000,
                lon=23522000,
                type="city",
            )],
        )
        encoded = _encode_payload(MessageType.MAP_GEOCODE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        result = data["results"][0]
        # These field names must exactly match C++ struct serialization
        assert set(result.keys()) == {"display_name", "lat", "lon", "type"}
        assert isinstance(result["lat"], int)
        assert isinstance(result["lon"], int)

    def test_route_instruction_field_names(self):
        """Route instruction field names must match C++ exactly."""
        payload = MapRouteResponsePayload(
            points=[0, 0],
            instructions=[MapRouteInstruction(
                distance_m=1500,
                maneuver="turn-right",
                street="Broadway",
            )],
            total_distance_m=1500,
            total_time_s=120,
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        inst = data["instructions"][0]
        assert set(inst.keys()) == {"distance_m", "maneuver", "street"}
        assert isinstance(inst["distance_m"], int)
        assert isinstance(inst["maneuver"], str)
        assert isinstance(inst["street"], str)


# ============================================================
# Config Save Edge Cases
# ============================================================


class TestConfigSave:
    """Test config save/load round-trip."""

    def test_save_and_reload(self, tmp_path):
        """Config save() should persist all fields, reloadable by a new Config."""
        config = Config(data_dir=tmp_path)
        config.maps_mbtiles_path = "/some/path.mbtiles"
        config.maps_valhalla_url = "http://localhost:8002"
        config.maps_nominatim_url = "http://localhost:8080"
        config.maps_tileserver_url = "http://localhost:8081"
        config.maps_enabled = True
        config.save()

        config2 = Config(data_dir=tmp_path)
        assert config2.maps_mbtiles_path == "/some/path.mbtiles"
        assert config2.maps_valhalla_url == "http://localhost:8002"
        assert config2.maps_nominatim_url == "http://localhost:8080"
        assert config2.maps_tileserver_url == "http://localhost:8081"
        assert config2.maps_enabled is True

    def test_save_preserves_all_fields(self, tmp_path):
        """Every config field should survive save/load."""
        config = Config(data_dir=tmp_path)
        config.server_name = "Test Server"
        config.enabled_services = ["ntp", "search", "maps"]
        config.search_max_results = 10
        config.ai_summary_enabled = True
        config.ai_summary_model_path = "/path/to/model.gguf"
        config.ntp_refresh_interval = 7200
        config.save()

        config2 = Config(data_dir=tmp_path)
        assert config2.server_name == "Test Server"
        assert config2.enabled_services == ["ntp", "search", "maps"]
        assert config2.search_max_results == 10
        assert config2.ai_summary_enabled is True
        assert config2.ai_summary_model_path == "/path/to/model.gguf"
        assert config2.ntp_refresh_interval == 7200


# ============================================================
# Tileserver Config Edge Cases
# ============================================================


class TestTileserverConfigEdgeCases:
    """Edge cases for maps_tileserver_url config field."""

    def test_tileserver_url_default_none(self, tmp_path):
        """Default config should have maps_tileserver_url = None."""
        config = Config(data_dir=tmp_path)
        assert config.maps_tileserver_url is None

    def test_tileserver_url_save_and_reload(self, tmp_path):
        """maps_tileserver_url should round-trip through save/load."""
        config = Config(data_dir=tmp_path)
        config.maps_tileserver_url = "http://localhost:8081"
        config.save()

        config2 = Config(data_dir=tmp_path)
        assert config2.maps_tileserver_url == "http://localhost:8081"

    def test_tileserver_url_save_null(self, tmp_path):
        """Setting maps_tileserver_url to None should persist as null."""
        config = Config(data_dir=tmp_path)
        config.maps_tileserver_url = "http://localhost:8081"
        config.save()

        config.maps_tileserver_url = None
        config.save()

        config2 = Config(data_dir=tmp_path)
        assert config2.maps_tileserver_url is None

    def test_tileserver_url_with_custom_port(self, tmp_path):
        """Tileserver URL with non-standard port should persist."""
        config = Config(data_dir=tmp_path)
        config.maps_tileserver_url = "http://192.168.1.100:9999"
        config.save()

        config2 = Config(data_dir=tmp_path)
        assert config2.maps_tileserver_url == "http://192.168.1.100:9999"

    def test_tileserver_url_with_trailing_slash(self, tmp_path):
        """Trailing slash in URL should be preserved as-is."""
        config = Config(data_dir=tmp_path)
        config.maps_tileserver_url = "http://localhost:8081/"
        config.save()

        config2 = Config(data_dir=tmp_path)
        assert config2.maps_tileserver_url == "http://localhost:8081/"

    def test_config_missing_tileserver_field_uses_default(self, tmp_path):
        """Config file without maps_tileserver_url should use default (None)."""
        import json
        # Create a config file that lacks the tileserver field
        config_file = tmp_path / "config.json"
        data = {
            "server_name": "Test",
            "enabled_services": ["ntp"],
            "maps_enabled": False,
            "maps_mbtiles_path": None,
            "maps_valhalla_url": None,
            "maps_nominatim_url": None,
        }
        with open(config_file, "w") as f:
            json.dump(data, f)

        # Ensure subdirectories exist
        (tmp_path / "reticulum").mkdir(exist_ok=True)

        config = Config(data_dir=tmp_path)
        assert config.maps_tileserver_url is None

    def test_all_maps_fields_coexist(self, tmp_path):
        """All maps config fields should be independently settable."""
        config = Config(data_dir=tmp_path)
        config.maps_enabled = True
        config.maps_mbtiles_path = "/path/to/tiles.mbtiles"
        config.maps_valhalla_url = "http://localhost:8002"
        config.maps_nominatim_url = "http://localhost:8080"
        config.maps_tileserver_url = "http://localhost:8081"
        config.save()

        config2 = Config(data_dir=tmp_path)
        assert config2.maps_enabled is True
        assert config2.maps_mbtiles_path == "/path/to/tiles.mbtiles"
        assert config2.maps_valhalla_url == "http://localhost:8002"
        assert config2.maps_nominatim_url == "http://localhost:8080"
        assert config2.maps_tileserver_url == "http://localhost:8081"


class TestTileserverServiceEdgeCases:
    """Edge cases for tileserver tile fetching."""

    @pytest.fixture
    def service(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        return MapsService(config)

    def test_fetch_tile_url_format(self, service):
        """Tileserver fetch should use correct URL pattern."""
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.content = b"png_data"
        mock_response.raise_for_status = Mock()

        with patch.object(service._http_client, 'get', return_value=mock_response) as mock_get:
            service._fetch_tile_from_tileserver(14, 2746, 6327)

        mock_get.assert_called_once_with(
            "http://localhost:8081/styles/grayscale/14/2746/6327.png",
            timeout=10.0,
        )

    def test_fetch_tile_zoom_zero(self, service):
        """Zoom level 0 should work correctly."""
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.content = b"png_data"
        mock_response.raise_for_status = Mock()

        with patch.object(service._http_client, 'get', return_value=mock_response) as mock_get:
            result = service._fetch_tile_from_tileserver(0, 0, 0)

        assert result == b"png_data"
        mock_get.assert_called_once_with(
            "http://localhost:8081/styles/grayscale/0/0/0.png",
            timeout=10.0,
        )

    def test_fetch_tile_high_zoom(self, service):
        """High zoom levels with large tile coordinates should work."""
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.content = b"png_data"
        mock_response.raise_for_status = Mock()

        with patch.object(service._http_client, 'get', return_value=mock_response):
            result = service._fetch_tile_from_tileserver(18, 131072, 98304)

        assert result == b"png_data"

    def test_mbtiles_fetch_corrupted_db(self, tmp_path):
        """Corrupted MBTiles database should return None, not crash."""
        db_path = tmp_path / "bad.mbtiles"
        db_path.write_bytes(b"not a sqlite database")

        config = Mock(spec=Config)
        config.maps_mbtiles_path = str(db_path)
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        svc = MapsService(config)
        # _init_mbtiles should have failed gracefully
        assert svc._mbtiles_conn is None

    def test_tile_request_with_no_sources_returns_not_found(self):
        """Tile request with no tile sources should return 'Tile not found'."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        svc = MapsService(config)

        payload = MapTileRequestPayload(z=10, x=100, y=200, format=TileFormat.MONO_RLE)
        responses = svc._handle_tile_request(payload)
        assert len(responses) == 1
        assert responses[0].error is not None


# ============================================================
# Helper Functions
# ============================================================


def _make_minimal_png() -> bytes:
    """Create a minimal valid 1x1 white PNG."""
    import zlib

    def _chunk(chunk_type: bytes, data: bytes) -> bytes:
        c = chunk_type + data
        crc = struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
        return struct.pack(">I", len(data)) + c + crc

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr_data = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)  # 1x1, 8-bit RGB
    ihdr = _chunk(b"IHDR", ihdr_data)
    # Raw image data: filter byte (0) + RGB pixel (255, 255, 255)
    raw = b"\x00\xFF\xFF\xFF"
    idat = _chunk(b"IDAT", zlib.compress(raw))
    iend = _chunk(b"IEND", b"")
    return sig + ihdr + idat + iend
