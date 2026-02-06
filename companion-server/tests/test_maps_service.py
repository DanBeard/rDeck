"""Tests for Maps service and protocol."""

import io
import sqlite3
import struct
import zlib
import pytest
from unittest.mock import Mock, patch, MagicMock, PropertyMock
from pathlib import Path

from companion_server.services.maps_service import (
    MapsService,
    _rle_encode,
    _rle_decode,
    _floyd_steinberg_dither,
    _image_to_packed_bits,
)
from companion_server.protocol.messages import (
    MessageType,
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
from companion_server.config import Config


class TestRLEEncoding:
    """Test RLE compression/decompression."""

    def test_rle_encode_simple(self):
        # 3 bytes of 0xFF followed by 2 bytes of 0x00
        data = bytes([0xFF, 0xFF, 0xFF, 0x00, 0x00])
        encoded = _rle_encode(data)
        # Should encode as [3, 0xFF, 2, 0x00]
        assert encoded == bytes([3, 0xFF, 2, 0x00])

    def test_rle_decode_simple(self):
        encoded = bytes([3, 0xFF, 2, 0x00])
        decoded = _rle_decode(encoded)
        assert decoded == bytes([0xFF, 0xFF, 0xFF, 0x00, 0x00])

    def test_rle_roundtrip(self):
        original = bytes([0xAA] * 100 + [0x55] * 50 + [0x00] * 200)
        encoded = _rle_encode(original)
        decoded = _rle_decode(encoded)
        assert decoded == original

    def test_rle_empty(self):
        assert _rle_encode(b"") == b""
        assert _rle_decode(b"") == b""

    def test_rle_long_run(self):
        # Runs > 255 should be split
        original = bytes([0xFF] * 300)
        encoded = _rle_encode(original)
        decoded = _rle_decode(encoded)
        assert decoded == original


class TestMapTilePayloads:
    """Test MapTileRequest/Response serialization."""

    def test_tile_request_serialize_deserialize(self):
        original = MapTileRequestPayload(z=14, x=2746, y=6327, format=TileFormat.MONO_RLE)

        encoded = _encode_payload(MessageType.MAP_TILE_REQUEST, original)
        decoded = _decode_payload(MessageType.MAP_TILE_REQUEST, encoded)

        assert decoded.z == 14
        assert decoded.x == 2746
        assert decoded.y == 6327
        assert decoded.format == TileFormat.MONO_RLE

    def test_tile_response_serialize_deserialize(self):
        original = MapTileResponsePayload(
            z=14,
            x=2746,
            y=6327,
            format=TileFormat.MONO_RLE,
            chunk_index=0,
            total_chunks=3,
            data=bytes([0x01, 0xFF, 0x02, 0x00]),
        )

        encoded = _encode_payload(MessageType.MAP_TILE_RESPONSE, original)
        decoded = _decode_payload(MessageType.MAP_TILE_RESPONSE, encoded)

        assert decoded.z == 14
        assert decoded.x == 2746
        assert decoded.y == 6327
        assert decoded.chunk_index == 0
        assert decoded.total_chunks == 3
        assert decoded.data == bytes([0x01, 0xFF, 0x02, 0x00])

    def test_tile_response_with_error(self):
        original = MapTileResponsePayload(
            z=14, x=9999, y=9999,
            format=TileFormat.MONO_RLE,
            chunk_index=0, total_chunks=1,
            data=b"",
            error="Tile not found"
        )

        encoded = _encode_payload(MessageType.MAP_TILE_RESPONSE, original)
        decoded = _decode_payload(MessageType.MAP_TILE_RESPONSE, encoded)

        assert decoded.error == "Tile not found"


class TestMapRoutePayloads:
    """Test MapRouteRequest/Response serialization."""

    def test_route_request_serialize_deserialize(self):
        original = MapRouteRequestPayload(
            start_lat=377749000,
            start_lon=-1224194000,
            end_lat=377850000,
            end_lon=-1224000000,
            mode=TravelMode.BIKE
        )

        encoded = _encode_payload(MessageType.MAP_ROUTE_REQUEST, original)
        decoded = _decode_payload(MessageType.MAP_ROUTE_REQUEST, encoded)

        assert decoded.start_lat == 377749000
        assert decoded.start_lon == -1224194000
        assert decoded.end_lat == 377850000
        assert decoded.end_lon == -1224000000
        assert decoded.mode == TravelMode.BIKE

    def test_route_response_serialize_deserialize(self):
        original = MapRouteResponsePayload(
            points=[377749000, -1224194000, 377750000, -1224190000],
            instructions=[
                MapRouteInstruction(distance_m=100, maneuver="start", street="Main St"),
                MapRouteInstruction(distance_m=200, maneuver="turn-left", street="Oak Ave"),
            ],
            total_distance_m=300,
            total_time_s=180
        )

        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, original)
        decoded = _decode_payload(MessageType.MAP_ROUTE_RESPONSE, encoded)

        assert len(decoded.points) == 4
        assert decoded.points[0] == 377749000
        assert len(decoded.instructions) == 2
        assert decoded.instructions[0].maneuver == "start"
        assert decoded.instructions[1].street == "Oak Ave"
        assert decoded.total_distance_m == 300
        assert decoded.total_time_s == 180

    def test_route_response_with_error(self):
        original = MapRouteResponsePayload(
            points=[],
            error="No route found"
        )

        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, original)
        decoded = _decode_payload(MessageType.MAP_ROUTE_RESPONSE, encoded)

        assert decoded.error == "No route found"
        assert len(decoded.points) == 0


class TestMapGeocodePayloads:
    """Test MapGeocodeRequest/Response serialization."""

    def test_geocode_request_serialize_deserialize(self):
        original = MapGeocodeRequestPayload(
            query="1600 Amphitheatre Parkway",
            bias_lat=377749000,
            bias_lon=-1224194000,
            max_results=3
        )

        encoded = _encode_payload(MessageType.MAP_GEOCODE_REQUEST, original)
        decoded = _decode_payload(MessageType.MAP_GEOCODE_REQUEST, encoded)

        assert decoded.query == "1600 Amphitheatre Parkway"
        assert decoded.bias_lat == 377749000
        assert decoded.bias_lon == -1224194000
        assert decoded.max_results == 3

    def test_geocode_request_without_bias(self):
        original = MapGeocodeRequestPayload(
            query="Empire State Building",
            max_results=5
        )

        encoded = _encode_payload(MessageType.MAP_GEOCODE_REQUEST, original)
        decoded = _decode_payload(MessageType.MAP_GEOCODE_REQUEST, encoded)

        assert decoded.query == "Empire State Building"
        assert decoded.bias_lat is None
        assert decoded.bias_lon is None

    def test_geocode_response_serialize_deserialize(self):
        original = MapGeocodeResponsePayload(
            query="coffee",
            results=[
                MapGeocodeResult(
                    display_name="Starbucks, 123 Main St",
                    lat=377749000,
                    lon=-1224194000,
                    type="cafe"
                ),
                MapGeocodeResult(
                    display_name="Peet's Coffee, 456 Oak Ave",
                    lat=377750000,
                    lon=-1224190000,
                    type="cafe"
                ),
            ]
        )

        encoded = _encode_payload(MessageType.MAP_GEOCODE_RESPONSE, original)
        decoded = _decode_payload(MessageType.MAP_GEOCODE_RESPONSE, encoded)

        assert decoded.query == "coffee"
        assert len(decoded.results) == 2
        assert decoded.results[0].display_name == "Starbucks, 123 Main St"
        assert decoded.results[0].lat == 377749000
        assert decoded.results[0].type == "cafe"

    def test_geocode_response_with_error(self):
        original = MapGeocodeResponsePayload(
            query="test",
            error="Geocoding service unavailable"
        )

        encoded = _encode_payload(MessageType.MAP_GEOCODE_RESPONSE, original)
        decoded = _decode_payload(MessageType.MAP_GEOCODE_RESPONSE, encoded)

        assert decoded.error == "Geocoding service unavailable"


class TestMapsService:
    """Test MapsService class."""

    @pytest.fixture
    def config(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None  # No MBTiles for basic tests
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        return config

    @pytest.fixture
    def service(self, config):
        return MapsService(config)

    def test_name(self, service):
        assert service.name == "maps"

    def test_available_false_without_mbtiles(self, service):
        # Service should report unavailable without MBTiles
        assert not service.available


class TestMessageTypeValues:
    """Test that message type values match C++ exactly."""

    def test_map_tile_request_value(self):
        assert int(MessageType.MAP_TILE_REQUEST) == 0x30

    def test_map_tile_response_value(self):
        assert int(MessageType.MAP_TILE_RESPONSE) == 0x31

    def test_map_route_request_value(self):
        assert int(MessageType.MAP_ROUTE_REQUEST) == 0x33

    def test_map_route_response_value(self):
        assert int(MessageType.MAP_ROUTE_RESPONSE) == 0x34

    def test_map_geocode_request_value(self):
        assert int(MessageType.MAP_GEOCODE_REQUEST) == 0x35

    def test_map_geocode_response_value(self):
        assert int(MessageType.MAP_GEOCODE_RESPONSE) == 0x36


class TestTileFormatEnum:
    """Test TileFormat enum values."""

    def test_mono_rle_value(self):
        assert int(TileFormat.MONO_RLE) == 0

    def test_raw_1bit_value(self):
        assert int(TileFormat.RAW_1BIT) == 1


class TestTravelModeEnum:
    """Test TravelMode enum values."""

    def test_walk_value(self):
        assert int(TravelMode.WALK) == 0

    def test_bike_value(self):
        assert int(TravelMode.BIKE) == 1

    def test_car_value(self):
        assert int(TravelMode.CAR) == 2


class TestCrossCompatibility:
    """Test that Python encoding matches what C++ expects."""

    def test_tile_request_field_names(self):
        """Verify field names match C++ exactly."""
        import msgpack

        payload = MapTileRequestPayload(z=14, x=1000, y=2000, format=TileFormat.MONO_RLE)
        encoded = _encode_payload(MessageType.MAP_TILE_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        # C++ expects these exact field names
        assert "z" in data
        assert "x" in data
        assert "y" in data
        assert "format" in data

    def test_route_response_field_names(self):
        """Verify route response field names match C++ exactly."""
        import msgpack

        payload = MapRouteResponsePayload(
            points=[100, 200],
            instructions=[MapRouteInstruction(distance_m=50, maneuver="start", street="Main")],
            total_distance_m=100,
            total_time_s=60
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        # C++ expects these exact field names
        assert "points" in data
        assert "instructions" in data
        assert "total_distance_m" in data
        assert "total_time_s" in data

        # Check instruction structure
        assert data["instructions"][0]["distance_m"] == 50
        assert data["instructions"][0]["maneuver"] == "start"
        assert data["instructions"][0]["street"] == "Main"

    def test_geocode_result_field_names(self):
        """Verify geocode result field names match C++ exactly."""
        import msgpack

        payload = MapGeocodeResponsePayload(
            query="test",
            results=[MapGeocodeResult(
                display_name="Test Place",
                lat=100000000,
                lon=200000000,
                type="poi"
            )]
        )
        encoded = _encode_payload(MessageType.MAP_GEOCODE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        # C++ expects these exact field names
        assert "query" in data
        assert "results" in data

        result = data["results"][0]
        assert "display_name" in result
        assert "lat" in result
        assert "lon" in result
        assert "type" in result


# ============================================================
# Helper Functions
# ============================================================


def _make_minimal_png() -> bytes:
    """Create a minimal valid 1x1 white PNG."""
    def _chunk(chunk_type: bytes, data: bytes) -> bytes:
        c = chunk_type + data
        crc = struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
        return struct.pack(">I", len(data)) + c + crc

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr_data = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)  # 1x1, 8-bit RGB
    ihdr = _chunk(b"IHDR", ihdr_data)
    raw = b"\x00\xFF\xFF\xFF"
    idat = _chunk(b"IDAT", zlib.compress(raw))
    iend = _chunk(b"IEND", b"")
    return sig + ihdr + idat + iend


def _make_test_mbtiles(path: Path) -> Path:
    """Create a minimal valid MBTiles file with one test tile."""
    conn = sqlite3.connect(str(path))
    conn.execute("CREATE TABLE metadata (name TEXT, value TEXT)")
    conn.execute("""CREATE TABLE tiles (
        zoom_level INTEGER, tile_column INTEGER,
        tile_row INTEGER, tile_data BLOB
    )""")
    conn.execute("INSERT INTO metadata VALUES ('name', 'test')")
    conn.execute("INSERT INTO metadata VALUES ('format', 'png')")
    png_data = _make_minimal_png()
    # z=10, x=512, y=512 -> TMS y = (1<<10) - 1 - 512 = 511
    conn.execute("INSERT INTO tiles VALUES (10, 512, 511, ?)", (png_data,))
    conn.commit()
    conn.close()
    return path


# ============================================================
# Tileserver Integration Tests
# ============================================================


class TestTileserverConfig:
    """Test MapsService initialization with tileserver URL."""

    def test_init_with_tileserver_url(self):
        """Service should store tileserver URL from config."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)
        assert svc._tileserver_url == "http://localhost:8081"

    def test_init_without_tileserver_url(self):
        """Service should handle missing tileserver URL gracefully."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        svc = MapsService(config)
        assert svc._tileserver_url is None

    def test_available_with_tileserver_only(self):
        """Service should be available with tileserver URL even without MBTiles."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)
        # Available requires PIL + (mbtiles OR tileserver)
        try:
            from PIL import Image
            assert svc.available is True
        except ImportError:
            assert svc.available is False  # PIL missing blocks availability

    def test_available_with_neither_source(self):
        """Service should be unavailable with no tile sources."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        svc = MapsService(config)
        assert svc.available is False

    def test_reload_config_updates_tileserver_url(self):
        """reload_config should update tileserver URL."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        svc = MapsService(config)
        assert svc._tileserver_url is None

        new_config = Mock(spec=Config)
        new_config.maps_mbtiles_path = None
        new_config.maps_valhalla_url = None
        new_config.maps_nominatim_url = None
        new_config.maps_tileserver_url = "http://localhost:9090"
        svc.reload_config(new_config)
        assert svc._tileserver_url == "http://localhost:9090"

    def test_reload_config_clears_tileserver_url(self):
        """reload_config should allow clearing tileserver URL."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)
        assert svc._tileserver_url == "http://localhost:8081"

        new_config = Mock(spec=Config)
        new_config.maps_mbtiles_path = None
        new_config.maps_valhalla_url = None
        new_config.maps_nominatim_url = None
        new_config.maps_tileserver_url = None
        svc.reload_config(new_config)
        assert svc._tileserver_url is None


class TestTileserverFetching:
    """Test HTTP tile fetching from tileserver-gl."""

    @pytest.fixture
    def service_with_tileserver(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        return MapsService(config)

    @pytest.fixture
    def service_no_tileserver(self):
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        return MapsService(config)

    def test_fetch_tile_success(self, service_with_tileserver):
        """Tileserver fetch should return PNG bytes on success."""
        fake_png = _make_minimal_png()
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.content = fake_png
        mock_response.raise_for_status = Mock()

        with patch.object(service_with_tileserver._http_client, 'get', return_value=mock_response) as mock_get:
            result = service_with_tileserver._fetch_tile_from_tileserver(10, 512, 512)

        assert result == fake_png
        mock_get.assert_called_once_with(
            "http://localhost:8081/styles/grayscale/10/512/512.png",
            timeout=10.0,
        )

    def test_fetch_tile_http_error(self, service_with_tileserver):
        """HTTP error should return None, not raise."""
        import httpx
        with patch.object(
            service_with_tileserver._http_client, 'get',
            side_effect=httpx.ConnectError("Connection refused")
        ):
            result = service_with_tileserver._fetch_tile_from_tileserver(10, 512, 512)
        assert result is None

    def test_fetch_tile_404(self, service_with_tileserver):
        """404 response should return None (raise_for_status raises)."""
        import httpx
        mock_response = Mock()
        mock_response.raise_for_status = Mock(
            side_effect=httpx.HTTPStatusError("404", request=Mock(), response=Mock())
        )
        with patch.object(service_with_tileserver._http_client, 'get', return_value=mock_response):
            result = service_with_tileserver._fetch_tile_from_tileserver(10, 512, 512)
        assert result is None

    def test_fetch_tile_no_tileserver_url(self, service_no_tileserver):
        """Without tileserver URL, fetch should return None immediately."""
        result = service_no_tileserver._fetch_tile_from_tileserver(10, 512, 512)
        assert result is None

    def test_fetch_tile_no_http_client(self):
        """Without httpx, fetch should return None."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)
        svc._http_client = None  # Simulate httpx not available
        result = svc._fetch_tile_from_tileserver(10, 512, 512)
        assert result is None

    def test_fetch_tile_timeout(self, service_with_tileserver):
        """Timeout should return None, not raise."""
        import httpx
        with patch.object(
            service_with_tileserver._http_client, 'get',
            side_effect=httpx.ReadTimeout("timeout")
        ):
            result = service_with_tileserver._fetch_tile_from_tileserver(10, 512, 512)
        assert result is None


class TestMBTilesFetching:
    """Test the extracted _fetch_tile_from_mbtiles method."""

    @pytest.fixture
    def mbtiles_path(self, tmp_path):
        return _make_test_mbtiles(tmp_path / "test.mbtiles")

    def test_fetch_existing_tile(self, mbtiles_path):
        """Should return tile data for existing coordinates."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = str(mbtiles_path)
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        svc = MapsService(config)

        result = svc._fetch_tile_from_mbtiles(10, 512, 512)
        assert result is not None
        assert len(result) > 0
        # Should be valid PNG data
        assert result[:4] == b"\x89PNG"

    def test_fetch_missing_tile(self, mbtiles_path):
        """Should return None for non-existent tile."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = str(mbtiles_path)
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        svc = MapsService(config)

        result = svc._fetch_tile_from_mbtiles(5, 999, 999)
        assert result is None

    def test_fetch_no_connection(self):
        """Should return None when no MBTiles connection."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = None
        svc = MapsService(config)

        result = svc._fetch_tile_from_mbtiles(10, 512, 512)
        assert result is None


class TestTileFallbackLogic:
    """Test tile request fallback: tileserver -> MBTiles -> error."""

    @pytest.fixture
    def mbtiles_path(self, tmp_path):
        return _make_test_mbtiles(tmp_path / "test.mbtiles")

    def test_tileserver_first_then_mbtiles_fallback(self, mbtiles_path):
        """When tileserver fails, should fall back to MBTiles."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = str(mbtiles_path)
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)

        # Tileserver returns None (failure), MBTiles has the tile
        with patch('companion_server.services.maps_service.PIL_AVAILABLE', True):
            with patch.object(svc, '_fetch_tile_from_tileserver', return_value=None):
                with patch.object(svc, '_fetch_tile_from_mbtiles', return_value=b"tile_data") as mock_mbtiles:
                    with patch.object(svc, '_process_tile', return_value=b"processed") as mock_process:
                        responses = svc._handle_tile_request(
                            MapTileRequestPayload(z=10, x=512, y=512, format=TileFormat.MONO_RLE)
                        )

        mock_mbtiles.assert_called_once_with(10, 512, 512)
        mock_process.assert_called_once_with(b"tile_data", TileFormat.MONO_RLE)
        assert responses[0].error is None

    def test_tileserver_success_skips_mbtiles(self):
        """When tileserver succeeds, should not query MBTiles."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)

        fake_png = _make_minimal_png()
        with patch('companion_server.services.maps_service.PIL_AVAILABLE', True):
            with patch.object(svc, '_fetch_tile_from_tileserver', return_value=fake_png):
                with patch.object(svc, '_fetch_tile_from_mbtiles') as mock_mbtiles:
                    with patch.object(svc, '_process_tile', return_value=b"processed"):
                        responses = svc._handle_tile_request(
                            MapTileRequestPayload(z=10, x=512, y=512, format=TileFormat.MONO_RLE)
                        )

        mock_mbtiles.assert_not_called()
        assert responses[0].error is None

    def test_both_sources_fail_returns_error(self):
        """When both tileserver and MBTiles fail, should return error."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)

        with patch('companion_server.services.maps_service.PIL_AVAILABLE', True):
            with patch.object(svc, '_fetch_tile_from_tileserver', return_value=None):
                with patch.object(svc, '_fetch_tile_from_mbtiles', return_value=None):
                    responses = svc._handle_tile_request(
                        MapTileRequestPayload(z=10, x=999, y=999, format=TileFormat.MONO_RLE)
                    )

        assert len(responses) == 1
        assert responses[0].error == "Tile not found"

    def test_no_pil_returns_error(self):
        """Without PIL, should return error before any fetch."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)

        with patch('companion_server.services.maps_service.PIL_AVAILABLE', False):
            responses = svc._handle_tile_request(
                MapTileRequestPayload(z=10, x=512, y=512, format=TileFormat.MONO_RLE)
            )

        assert len(responses) == 1
        assert "PIL" in responses[0].error

    def test_tileserver_only_no_mbtiles(self):
        """Service with only tileserver (no MBTiles) should work."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)

        fake_png = _make_minimal_png()
        with patch('companion_server.services.maps_service.PIL_AVAILABLE', True):
            with patch.object(svc, '_fetch_tile_from_tileserver', return_value=fake_png):
                with patch.object(svc, '_process_tile', return_value=b"processed"):
                    responses = svc._handle_tile_request(
                        MapTileRequestPayload(z=10, x=512, y=512, format=TileFormat.MONO_RLE)
                    )

        assert responses[0].error is None
        assert responses[0].data == b"processed"

    def test_process_tile_exception_returns_error(self):
        """Exception during tile processing should return error, not crash."""
        config = Mock(spec=Config)
        config.maps_mbtiles_path = None
        config.maps_valhalla_url = None
        config.maps_nominatim_url = None
        config.maps_tileserver_url = "http://localhost:8081"
        svc = MapsService(config)

        with patch('companion_server.services.maps_service.PIL_AVAILABLE', True):
            with patch.object(svc, '_fetch_tile_from_tileserver', return_value=b"bad data"):
                with patch.object(svc, '_process_tile', side_effect=Exception("decode error")):
                    responses = svc._handle_tile_request(
                        MapTileRequestPayload(z=10, x=512, y=512, format=TileFormat.MONO_RLE)
                    )

        assert len(responses) == 1
        assert "decode error" in responses[0].error
