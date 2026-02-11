"""Maps service - provides offline map tiles, routing, and geocoding."""

import io
import logging
import sqlite3
from pathlib import Path
from typing import Optional, Union

from .base_service import BaseService
from ..protocol import (
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
from ..config import Config

logger = logging.getLogger(__name__)

# Optional PIL import for tile processing
try:
    from PIL import Image
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    Image = None

# Optional httpx for external services (Valhalla, Nominatim)
try:
    import httpx
    HTTPX_AVAILABLE = True
except ImportError:
    HTTPX_AVAILABLE = False


# Constants
TILE_SIZE = 128  # Downsampled tile size (from 256x256)


def _rle_encode(data: bytes) -> bytes:
    """Run-length encode binary data.

    Format: [count, value, count, value, ...]
    count is 1 byte (1-255), value is 1 byte
    For runs > 255, multiple entries are used.
    """
    if not data:
        return b""

    result = bytearray()
    i = 0

    while i < len(data):
        current = data[i]
        count = 1

        # Count consecutive identical bytes
        while i + count < len(data) and data[i + count] == current and count < 255:
            count += 1

        result.append(count)
        result.append(current)
        i += count

    return bytes(result)


def _rle_decode(data: bytes) -> bytes:
    """Decode RLE-encoded data."""
    if not data:
        return b""

    result = bytearray()
    i = 0

    while i + 1 < len(data):
        count = data[i]
        value = data[i + 1]
        result.extend([value] * count)
        i += 2

    return bytes(result)


def _floyd_steinberg_dither(image: "Image.Image") -> "Image.Image":
    """Apply Floyd-Steinberg dithering to convert image to 1-bit.

    Args:
        image: PIL Image in grayscale mode

    Returns:
        1-bit PIL Image
    """
    # Convert to grayscale if needed
    if image.mode != "L":
        image = image.convert("L")

    # Get pixel data as a mutable list
    width, height = image.size
    pixels = list(image.getdata())

    # Convert to 2D array for easier manipulation
    img_array = [[pixels[y * width + x] for x in range(width)] for y in range(height)]

    # Floyd-Steinberg dithering
    for y in range(height):
        for x in range(width):
            old_pixel = img_array[y][x]
            new_pixel = 255 if old_pixel > 127 else 0
            img_array[y][x] = new_pixel
            error = old_pixel - new_pixel

            # Distribute error to neighboring pixels
            if x + 1 < width:
                img_array[y][x + 1] = max(0, min(255, int(img_array[y][x + 1] + error * 7 / 16)))
            if y + 1 < height:
                if x > 0:
                    img_array[y + 1][x - 1] = max(0, min(255, int(img_array[y + 1][x - 1] + error * 3 / 16)))
                img_array[y + 1][x] = max(0, min(255, int(img_array[y + 1][x] + error * 5 / 16)))
                if x + 1 < width:
                    img_array[y + 1][x + 1] = max(0, min(255, int(img_array[y + 1][x + 1] + error * 1 / 16)))

    # Convert back to image
    flat_pixels = [img_array[y][x] for y in range(height) for x in range(width)]
    result = Image.new("L", (width, height))
    result.putdata(flat_pixels)

    # Convert to 1-bit
    return result.convert("1")


def _image_to_packed_bits(image: "Image.Image") -> bytes:
    """Convert 1-bit image to packed bytes (8 pixels per byte).

    Args:
        image: 1-bit PIL Image

    Returns:
        Packed bytes where each bit represents a pixel (1=white, 0=black)
    """
    if image.mode != "1":
        image = image.convert("1")

    width, height = image.size
    pixels = list(image.getdata())

    # Pack 8 pixels into each byte
    packed = bytearray()
    for i in range(0, len(pixels), 8):
        byte = 0
        for j in range(8):
            if i + j < len(pixels) and pixels[i + j]:
                byte |= (1 << (7 - j))
        packed.append(byte)

    return bytes(packed)


class MapsService(BaseService):
    """Offline maps service providing tiles, routing, and geocoding.

    Tiles are read from MBTiles files and dithered to 1-bit for e-ink display.
    Routing uses Valhalla if configured.
    Geocoding uses Nominatim if configured.
    """

    def __init__(self, config: Config):
        self.config = config
        self._mbtiles_conn: Optional[sqlite3.Connection] = None
        self._http_client: Optional["httpx.Client"] = None
        self._tileserver_url: Optional[str] = getattr(config, "maps_tileserver_url", None)

        # Initialize MBTiles connection if path configured
        mbtiles_path = getattr(config, "maps_mbtiles_path", None)
        if mbtiles_path and Path(mbtiles_path).exists():
            self._init_mbtiles(Path(mbtiles_path))
        elif not self._tileserver_url:
            logger.warning("Maps: no MBTiles path or tileserver URL configured")

        # Initialize HTTP client for routing/geocoding/tileserver services
        if HTTPX_AVAILABLE:
            self._http_client = httpx.Client(timeout=30.0)

    def _init_mbtiles(self, path: Path):
        """Initialize MBTiles SQLite connection."""
        try:
            self._mbtiles_conn = sqlite3.connect(str(path), check_same_thread=False)
            logger.info(f"Loaded MBTiles from {path}")

            # Log metadata
            cursor = self._mbtiles_conn.execute(
                "SELECT name, value FROM metadata"
            )
            metadata = dict(cursor.fetchall())
            logger.info(f"MBTiles metadata: {metadata.get('name', 'unknown')}")
        except Exception as e:
            logger.error(f"Failed to load MBTiles: {e}")
            self._mbtiles_conn = None

    @property
    def name(self) -> str:
        return "maps"

    @property
    def available(self) -> bool:
        """Check if maps service is available."""
        has_tiles = self._mbtiles_conn is not None or (
            self._tileserver_url is not None and HTTPX_AVAILABLE
        )
        return has_tiles and PIL_AVAILABLE

    def handle_request(
        self,
        payload: Union[MapTileRequestPayload, MapRouteRequestPayload, MapGeocodeRequestPayload],
        msg_type: MessageType
    ) -> Union[MapTileResponsePayload, MapRouteResponsePayload, MapGeocodeResponsePayload]:
        """Handle maps service request based on message type.

        Args:
            payload: Request payload
            msg_type: Message type to determine operation

        Returns:
            Appropriate response payload
        """
        if msg_type == MessageType.MAP_TILE_REQUEST:
            return self._handle_tile_request(payload)
        elif msg_type == MessageType.MAP_ROUTE_REQUEST:
            return self._handle_route_request(payload)
        elif msg_type == MessageType.MAP_GEOCODE_REQUEST:
            return self._handle_geocode_request(payload)
        else:
            raise ValueError(f"Unknown maps message type: {msg_type}")

    def _fetch_tile_from_tileserver(self, z: int, x: int, y: int) -> Optional[bytes]:
        """Fetch a rendered PNG tile from tileserver-gl.

        Args:
            z, x, y: Tile coordinates (XYZ scheme)

        Returns:
            PNG bytes or None on failure
        """
        if not self._tileserver_url or not self._http_client:
            return None

        try:
            url = f"{self._tileserver_url}/styles/grayscale/{z}/{x}/{y}.png"
            response = self._http_client.get(url, timeout=10.0)
            response.raise_for_status()
            return response.content
        except Exception as e:
            logger.debug(f"Tileserver fetch failed for {z}/{x}/{y}: {e}")
            return None

    def _fetch_tile_from_mbtiles(self, z: int, x: int, y: int) -> Optional[bytes]:
        """Fetch a tile from the local MBTiles SQLite database.

        Args:
            z, x, y: Tile coordinates (XYZ scheme, converted to TMS internally)

        Returns:
            Raw tile bytes or None if not found
        """
        if not self._mbtiles_conn:
            return None

        try:
            # MBTiles uses TMS y-coordinate (flipped from XYZ)
            tms_y = (1 << z) - 1 - y
            cursor = self._mbtiles_conn.execute(
                "SELECT tile_data FROM tiles WHERE zoom_level = ? AND tile_column = ? AND tile_row = ?",
                (z, x, tms_y)
            )
            row = cursor.fetchone()
            return row[0] if row else None
        except Exception as e:
            logger.debug(f"MBTiles fetch failed for {z}/{x}/{y}: {e}")
            return None

    def _handle_tile_request(self, payload: MapTileRequestPayload) -> MapTileResponsePayload:
        """Handle tile request - returns a single response payload.

        Tries tileserver-gl first (rendered vector tiles), falls back to MBTiles.
        Large payloads are transferred via Reticulum Resources automatically.

        Args:
            payload: Tile request with z, x, y coordinates

        Returns:
            Tile response payload
        """
        z, x, y = payload.z, payload.x, payload.y

        if not PIL_AVAILABLE:
            return MapTileResponsePayload(
                z=z, x=x, y=y,
                format=payload.format,
                data=b"",
                error="PIL not available for tile processing",
            )

        try:
            # Try tileserver first, then MBTiles
            tile_data = self._fetch_tile_from_tileserver(z, x, y)
            if tile_data is None:
                tile_data = self._fetch_tile_from_mbtiles(z, x, y)

            if tile_data is None:
                return MapTileResponsePayload(
                    z=z, x=x, y=y,
                    format=payload.format,
                    data=b"",
                    error="Tile not found",
                )

            # Process tile: decode PNG, resize, dither, compress
            processed_data = self._process_tile(tile_data, payload.format)

            return MapTileResponsePayload(
                z=z, x=x, y=y,
                format=payload.format,
                data=processed_data,
            )

        except Exception as e:
            logger.exception(f"Tile request error: {e}")
            return MapTileResponsePayload(
                z=z, x=x, y=y,
                format=payload.format,
                data=b"",
                error=str(e),
            )

    def _process_tile(self, tile_data: bytes, format: TileFormat) -> bytes:
        """Process raw tile data into 1-bit format.

        Args:
            tile_data: Raw PNG tile data from MBTiles
            format: Desired output format

        Returns:
            Processed tile data (RLE compressed or raw 1-bit)
        """
        # Load image from bytes
        image = Image.open(io.BytesIO(tile_data))

        # Resize from 256x256 to 128x128
        image = image.resize((TILE_SIZE, TILE_SIZE), Image.Resampling.LANCZOS)

        # Convert to grayscale
        image = image.convert("L")

        # Apply Floyd-Steinberg dithering to 1-bit
        dithered = _floyd_steinberg_dither(image)

        # Pack bits (128x128 = 16384 pixels = 2048 bytes)
        packed = _image_to_packed_bits(dithered)

        # Compress if requested
        if format == TileFormat.MONO_RLE:
            return _rle_encode(packed)
        else:
            return packed

    def _handle_route_request(self, payload: MapRouteRequestPayload) -> MapRouteResponsePayload:
        """Handle routing request via Valhalla.

        Args:
            payload: Route request with start/end coordinates

        Returns:
            Route response with points and instructions
        """
        valhalla_url = getattr(self.config, "maps_valhalla_url", None)

        if not valhalla_url or not self._http_client:
            return MapRouteResponsePayload(
                points=[],
                error="Routing service not configured",
            )

        try:
            # Convert coordinates from int32 * 1e7 to float
            start_lat = payload.start_lat / 1e7
            start_lon = payload.start_lon / 1e7
            end_lat = payload.end_lat / 1e7
            end_lon = payload.end_lon / 1e7

            # Map travel mode to Valhalla costing
            costing_map = {
                TravelMode.WALK: "pedestrian",
                TravelMode.BIKE: "bicycle",
                TravelMode.CAR: "auto",
            }
            costing = costing_map.get(payload.mode, "pedestrian")

            # Build Valhalla request
            request_body = {
                "locations": [
                    {"lat": start_lat, "lon": start_lon},
                    {"lat": end_lat, "lon": end_lon},
                ],
                "costing": costing,
                "directions_options": {
                    "units": "meters",
                },
            }

            response = self._http_client.post(
                f"{valhalla_url}/route",
                json=request_body,
            )
            response.raise_for_status()
            data = response.json()

            return self._parse_valhalla_response(data)

        except Exception as e:
            logger.exception(f"Routing error: {e}")
            return MapRouteResponsePayload(
                points=[],
                error=str(e),
            )

    VALHALLA_MANEUVER_TYPES = {
        0: "none", 1: "start", 2: "start-right", 3: "start-left",
        4: "destination", 5: "destination-right", 6: "destination-left",
        7: "becomes", 8: "continue", 9: "turn-slight-right",
        10: "turn-right", 11: "turn-sharp-right", 12: "u-turn-right",
        13: "u-turn-left", 14: "turn-sharp-left", 15: "turn-left",
        16: "turn-slight-left", 17: "ramp-straight", 18: "ramp-right",
        19: "ramp-left", 20: "exit-right", 21: "exit-left",
        22: "stay-straight", 23: "stay-right", 24: "stay-left",
        25: "merge", 26: "roundabout-enter", 27: "roundabout-exit",
        28: "ferry-enter", 29: "ferry-exit",
    }

    def _parse_valhalla_response(self, data: dict) -> MapRouteResponsePayload:
        """Parse Valhalla response into our format.

        Args:
            data: Valhalla JSON response

        Returns:
            Parsed route response
        """
        try:
            trip = data.get("trip", {})
            legs = trip.get("legs", [])
            summary = trip.get("summary", {})

            # Extract route points (all legs combined)
            points = []
            for leg in legs:
                shape = leg.get("shape", "")
                # Valhalla uses encoded polyline format
                decoded = self._decode_polyline(shape)
                for lat, lon in decoded:
                    # Convert to int32 * 1e7
                    points.append(int(lat * 1e7))
                    points.append(int(lon * 1e7))

            # Extract maneuvers as instructions
            instructions = []
            for leg in legs:
                for maneuver in leg.get("maneuvers", []):
                    instructions.append(MapRouteInstruction(
                        distance_m=int(maneuver.get("length", 0) * 1000),  # km to m
                        maneuver=self.VALHALLA_MANEUVER_TYPES.get(maneuver.get("type", 0), "continue"),
                        street=maneuver.get("street_names", [""])[0] if maneuver.get("street_names") else "",
                    ))

            return MapRouteResponsePayload(
                points=points,
                instructions=instructions,
                total_distance_m=int(summary.get("length", 0) * 1000),
                total_time_s=int(summary.get("time", 0)),
            )

        except Exception as e:
            logger.error(f"Failed to parse Valhalla response: {e}")
            return MapRouteResponsePayload(
                points=[],
                error=f"Failed to parse route: {e}",
            )

    def _decode_polyline(self, encoded: str, precision: int = 6) -> list[tuple[float, float]]:
        """Decode Google-style encoded polyline.

        Args:
            encoded: Encoded polyline string
            precision: Coordinate precision (6 for Valhalla)

        Returns:
            List of (lat, lon) tuples
        """
        inv = 1.0 / (10 ** precision)
        decoded = []
        previous = [0, 0]
        i = 0

        while i < len(encoded):
            for dim in range(2):
                shift = 0
                result = 0

                while True:
                    char = ord(encoded[i]) - 63
                    i += 1
                    result |= (char & 0x1F) << shift
                    shift += 5
                    if char < 0x20:
                        break

                if result & 1:
                    result = ~result
                result >>= 1
                previous[dim] += result

            decoded.append((previous[0] * inv, previous[1] * inv))

        return decoded

    # Max bytes for the geocode service payload (msgpack-encoded).
    # microReticulum lacks Resource support, so the full LXMF message
    # must fit in a single link packet.  LXMF LINK_PACKET_MAX_CONTENT
    # = 319 bytes; packed_payload overhead (array header + timestamp +
    # empty title/content + service fields keys) ≈ 61 bytes, leaving
    # ~274 bytes for the geocode payload.  Use 270 for safety margin.
    MAX_GEOCODE_PAYLOAD_BYTES = 270

    @staticmethod
    def _truncate_display_name(name: str, max_len: int = 50) -> str:
        """Truncate at a comma boundary to keep names readable."""
        if len(name) <= max_len:
            return name
        # Find the last comma before the limit
        truncated = name[:max_len]
        last_comma = truncated.rfind(",")
        if last_comma > 15:  # keep at least 15 chars
            return truncated[:last_comma]
        return truncated.rstrip() + "..."

    def _handle_geocode_request(self, payload: MapGeocodeRequestPayload) -> MapGeocodeResponsePayload:
        """Handle geocoding request via Nominatim.

        Args:
            payload: Geocode request with search query

        Returns:
            Geocode response with matching places
        """
        nominatim_url = getattr(self.config, "maps_nominatim_url", None)

        if not nominatim_url or not self._http_client:
            return MapGeocodeResponsePayload(
                query=payload.query,
                error="Geocoding service not configured",
            )

        try:
            # Cap results to keep response within single link packet
            max_results = min(payload.max_results, 3)

            params = {
                "q": payload.query,
                "format": "json",
                "limit": max_results,
                "addressdetails": 1,
            }

            # Add viewbox bias if coordinates provided
            if payload.bias_lat is not None and payload.bias_lon is not None:
                lat = payload.bias_lat / 1e7
                lon = payload.bias_lon / 1e7
                # Create a viewbox around the bias point (roughly 50km)
                params["viewbox"] = f"{lon - 0.5},{lat + 0.5},{lon + 0.5},{lat - 0.5}"
                params["bounded"] = 0  # Don't strictly limit to viewbox

            response = self._http_client.get(
                f"{nominatim_url}/search",
                params=params,
                headers={"User-Agent": "rDeck Companion Server"},
            )
            response.raise_for_status()
            data = response.json()

            results = []
            for item in data[:max_results]:
                results.append(MapGeocodeResult(
                    display_name=self._truncate_display_name(
                        item.get("display_name", "")
                    ),
                    lat=int(float(item.get("lat", 0)) * 1e7),
                    lon=int(float(item.get("lon", 0)) * 1e7),
                    type=item.get("type", "")[:12],
                ))

            # Verify encoded payload fits in a single link packet.
            # Progressively drop results if it's too large.
            import msgpack
            while len(results) > 0:
                trial = {
                    "query": payload.query,
                    "results": [
                        {"display_name": r.display_name, "lat": r.lat,
                         "lon": r.lon, "type": r.type}
                        for r in results
                    ],
                }
                if len(msgpack.packb(trial, use_bin_type=True)) <= self.MAX_GEOCODE_PAYLOAD_BYTES:
                    break
                results.pop()  # drop last result to fit

            return MapGeocodeResponsePayload(
                query=payload.query,
                results=results,
            )

        except Exception as e:
            logger.exception(f"Geocoding error: {e}")
            return MapGeocodeResponsePayload(
                query=payload.query,
                error=str(e),
            )

    def reload_mbtiles(self, path: str):
        """Close existing MBTiles connection and open a new one.

        Args:
            path: Path to the new MBTiles file
        """
        if self._mbtiles_conn:
            self._mbtiles_conn.close()
            self._mbtiles_conn = None

        p = Path(path)
        if p.exists():
            self._init_mbtiles(p)
        else:
            logger.warning(f"MBTiles file not found: {path}")

    def reload_config(self, config: Config):
        """Update service configuration without full restart.

        Args:
            config: Updated Config object
        """
        self.config = config

        # Update tileserver URL
        self._tileserver_url = getattr(config, "maps_tileserver_url", None)

        # Reload MBTiles if path changed
        mbtiles_path = getattr(config, "maps_mbtiles_path", None)
        if mbtiles_path:
            current_path = None
            if self._mbtiles_conn:
                # Can't easily get current path, just reload
                pass
            self.reload_mbtiles(mbtiles_path)

        # HTTP client is reused - Valhalla/Nominatim URLs are read from config at request time

    def close(self):
        """Clean up resources."""
        if self._mbtiles_conn:
            self._mbtiles_conn.close()
            self._mbtiles_conn = None
        if self._http_client:
            self._http_client.close()
            self._http_client = None
