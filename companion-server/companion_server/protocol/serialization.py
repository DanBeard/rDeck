"""Serialization helpers for service messages."""

import msgpack
from typing import Any

from .messages import (
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
)


def encode_service_fields(msg: ServiceMessage) -> dict:
    """Encode a ServiceMessage to LXMF fields dict.

    Returns a dict suitable for passing to LXMF message fields.
    """
    payload_bytes = _encode_payload(msg.msg_type, msg.payload)

    return {
        "msg_type": int(msg.msg_type),
        "service": msg.service,
        "payload": payload_bytes,
        "request_id": msg.request_id,
    }


def decode_service_fields(fields: dict) -> ServiceMessage:
    """Decode LXMF fields dict to a ServiceMessage.

    Args:
        fields: The LXMF message fields dict

    Returns:
        Decoded ServiceMessage with appropriate payload type
    """
    msg_type = MessageType(fields.get("msg_type", 0))
    service = fields.get("service", "")
    payload_bytes = fields.get("payload", b"")
    request_id = fields.get("request_id", 0)

    payload = _decode_payload(msg_type, payload_bytes)

    return ServiceMessage(
        msg_type=msg_type,
        service=service,
        payload=payload,
        request_id=request_id,
    )


def _encode_payload(msg_type: MessageType, payload: Any) -> bytes:
    """Encode payload based on message type."""
    if payload is None:
        return b""

    data: dict = {}

    if msg_type == MessageType.TRUST_OFFER:
        p: TrustOfferPayload = payload
        data = {"server_name": p.server_name, "services": p.services}

    elif msg_type == MessageType.TRUST_ACCEPT:
        p: TrustAcceptPayload = payload
        data = {"device_name": p.device_name}

    elif msg_type == MessageType.TRUST_REVOKE:
        p: TrustRevokePayload = payload
        data = {"reason": p.reason}

    elif msg_type == MessageType.NTP_REQUEST:
        p: NTPRequestPayload = payload
        data = {"client_timestamp": p.client_timestamp}

    elif msg_type == MessageType.NTP_RESPONSE:
        p: NTPResponsePayload = payload
        data = {
            "server_timestamp": p.server_timestamp,
            "client_timestamp": p.client_timestamp,
        }

    elif msg_type == MessageType.SEARCH_REQUEST:
        p: SearchRequestPayload = payload
        data = {"query": p.query, "max_results": p.max_results}

    elif msg_type == MessageType.SEARCH_RESPONSE:
        p: SearchResponsePayload = payload
        results = [
            {"title": r.title, "url": r.url, "snippet": r.snippet}
            for r in p.results
        ]
        data = {"query": p.query, "results": results, "error": p.error}

    return msgpack.packb(data, use_bin_type=True)


def _decode_payload(msg_type: MessageType, payload_bytes: bytes) -> Any:
    """Decode payload based on message type."""
    if not payload_bytes:
        return None

    data = msgpack.unpackb(payload_bytes, raw=False)

    if msg_type == MessageType.TRUST_OFFER:
        return TrustOfferPayload(
            server_name=data.get("server_name", ""),
            services=data.get("services", []),
        )

    elif msg_type == MessageType.TRUST_ACCEPT:
        return TrustAcceptPayload(device_name=data.get("device_name", ""))

    elif msg_type == MessageType.TRUST_REVOKE:
        return TrustRevokePayload(reason=data.get("reason"))

    elif msg_type == MessageType.NTP_REQUEST:
        return NTPRequestPayload(client_timestamp=data.get("client_timestamp", 0))

    elif msg_type == MessageType.NTP_RESPONSE:
        return NTPResponsePayload(
            server_timestamp=data.get("server_timestamp", 0),
            client_timestamp=data.get("client_timestamp", 0),
        )

    elif msg_type == MessageType.SEARCH_REQUEST:
        return SearchRequestPayload(
            query=data.get("query", ""),
            max_results=data.get("max_results", 5),
        )

    elif msg_type == MessageType.SEARCH_RESPONSE:
        results = [
            SearchResult(
                title=r.get("title", ""),
                url=r.get("url", ""),
                snippet=r.get("snippet", ""),
            )
            for r in data.get("results", [])
        ]
        return SearchResponsePayload(
            query=data.get("query", ""),
            results=results,
            error=data.get("error"),
        )

    return data
