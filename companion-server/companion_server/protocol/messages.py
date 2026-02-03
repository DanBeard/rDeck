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
