"""Tests for Maps service and protocol."""

import io
import pytest
from unittest.mock import Mock, patch, MagicMock
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
