"""Message type definitions for service protocol."""

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional, Any


class MessageType(IntEnum):
    """Service message types."""

    # Trust management
    TRUST_OFFER = 0x01
    TRUST_ACCEPT = 0x02
    TRUST_REVOKE = 0x03

    # NTP service
    NTP_REQUEST = 0x10
    NTP_RESPONSE = 0x11

    # Search service
    SEARCH_REQUEST = 0x20
    SEARCH_RESPONSE = 0x21

    # Maps service
    MAP_TILE_REQUEST = 0x30
    MAP_TILE_RESPONSE = 0x31
    MAP_ROUTE_REQUEST = 0x33
    MAP_ROUTE_RESPONSE = 0x34
    MAP_GEOCODE_REQUEST = 0x35
    MAP_GEOCODE_RESPONSE = 0x36


@dataclass
class ServiceMessage:
    """Base service message structure.

    This maps to the LXMF fields dict:
    {
        "msg_type": <uint8>,
        "service": <string>,
        "payload": <msgpack>,
        "request_id": <uint32>
    }
    """

    msg_type: MessageType
    service: str
    payload: Any
    request_id: int = 0


# Trust protocol payloads


@dataclass
class TrustOfferPayload:
    """Payload for TRUST_OFFER message.

    Server -> Device: "I want to serve you"
    """

    server_name: str
    services: list[str]  # List of services offered: ["ntp", "search"]


@dataclass
class TrustAcceptPayload:
    """Payload for TRUST_ACCEPT message.

    Device -> Server: "I accept your services"
    """

    device_name: str


@dataclass
class TrustRevokePayload:
    """Payload for TRUST_REVOKE message.

    Either direction: Revoke trust
    """

    reason: Optional[str] = None


# NTP service payloads


@dataclass
class NTPRequestPayload:
    """Payload for NTP_REQUEST message.

    Device -> Server: Request time sync
    """

    client_timestamp: int  # Client's current timestamp (epoch ms) for RTT calculation


@dataclass
class NTPResponsePayload:
    """Payload for NTP_RESPONSE message.

    Server -> Device: Time response
    """

    server_timestamp: int  # Server's current timestamp (epoch seconds)
    client_timestamp: int  # Echo back client's timestamp for RTT calculation


# Search service payloads


@dataclass
class SearchRequestPayload:
    """Payload for SEARCH_REQUEST message.

    Device -> Server: Search query
    """

    query: str
    max_results: int = 5
    ai_summary: bool = False  # Request AI-generated summary instead of raw results


@dataclass
class SearchResult:
    """A single search result."""

    title: str
    url: str
    snippet: str


@dataclass
class SearchResponsePayload:
    """Payload for SEARCH_RESPONSE message.

    Server -> Device: Search results
    """

    query: str
    results: list[SearchResult] = field(default_factory=list)
    error: Optional[str] = None
    summary: Optional[str] = None  # AI-generated summary (if requested and available)


# Maps service payloads


class TileFormat(IntEnum):
    """Tile format enum for map tiles."""

    MONO_RLE = 0  # 1-bit dithered, RLE compressed (default)
    RAW_1BIT = 1  # 1-bit dithered, uncompressed (128x128 = 2KB)


class TravelMode(IntEnum):
    """Travel mode for routing."""

    WALK = 0
    BIKE = 1
    CAR = 2


@dataclass
class MapTileRequestPayload:
    """Payload for MAP_TILE_REQUEST message.

    Device -> Server: Request a map tile
    """

    z: int  # Zoom level (typically 10-16)
    x: int  # Tile X coordinate
    y: int  # Tile Y coordinate
    format: TileFormat = TileFormat.MONO_RLE


@dataclass
class MapTileResponsePayload:
    """Payload for MAP_TILE_RESPONSE message.

    Server -> Device: Return map tile data
    Large payloads are transferred via Reticulum Resources automatically.
    """

    z: int
    x: int
    y: int
    format: TileFormat
    data: bytes  # RLE-compressed 1-bit tile data
    error: Optional[str] = None


@dataclass
class MapRouteRequestPayload:
    """Payload for MAP_ROUTE_REQUEST message.

    Device -> Server: Request route between two points
    Coordinates stored as int32 * 1e7 for precision without floats
    """

    start_lat: int  # Latitude * 1e7
    start_lon: int  # Longitude * 1e7
    end_lat: int
    end_lon: int
    mode: TravelMode = TravelMode.WALK


@dataclass
class MapRouteInstruction:
    """A turn-by-turn instruction."""

    distance_m: int  # Distance in meters to this maneuver
    maneuver: str  # "turn-left", "turn-right", "straight", "arrive", etc.
    street: str  # Street name (may be empty)
    bearing: int = 0  # Compass bearing after maneuver (0-360, 0=north)


@dataclass
class MapRouteResponsePayload:
    """Payload for MAP_ROUTE_RESPONSE message.

    Server -> Device: Return route with directions
    """

    points: list[int]  # Lat/lon pairs * 1e7 (alternating: lat0, lon0, lat1, lon1, ...)
    instructions: list[MapRouteInstruction] = field(default_factory=list)
    total_distance_m: int = 0  # Total route distance in meters
    total_time_s: int = 0  # Estimated time in seconds
    error: Optional[str] = None


@dataclass
class MapGeocodeRequestPayload:
    """Payload for MAP_GEOCODE_REQUEST message.

    Device -> Server: Search by address/place name
    """

    query: str  # Search query (address, place name, etc.)
    bias_lat: Optional[int] = None  # Optional: bias results near this lat * 1e7
    bias_lon: Optional[int] = None  # Optional: bias results near this lon * 1e7
    max_results: int = 5


@dataclass
class MapGeocodeResult:
    """A single geocode result."""

    display_name: str  # Full formatted address/name
    lat: int  # Latitude * 1e7
    lon: int  # Longitude * 1e7
    type: str  # Place type: "city", "street", "house", "poi", etc.


@dataclass
class MapGeocodeResponsePayload:
    """Payload for MAP_GEOCODE_RESPONSE message.

    Server -> Device: Return geocode results
    """

    query: str
    results: list[MapGeocodeResult] = field(default_factory=list)
    error: Optional[str] = None
