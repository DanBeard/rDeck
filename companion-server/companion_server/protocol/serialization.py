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
    PropSyncRequestPayload,
    PropSyncResponsePayload,
    PropMsgDeliverPayload,
    PropSubmitRequestPayload,
    PropSubmitResponsePayload,
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
        # Only include ai_summary if True (maintains backwards compatibility)
        if p.ai_summary:
            data["ai_summary"] = p.ai_summary

    elif msg_type == MessageType.SEARCH_RESPONSE:
        p: SearchResponsePayload = payload
        results = [
            {"title": r.title, "url": r.url, "snippet": r.snippet}
            for r in p.results
        ]
        data = {"query": p.query, "results": results, "error": p.error}
        # Only include summary if present (maintains backwards compatibility)
        if p.summary is not None:
            data["summary"] = p.summary

    # Maps service payloads
    elif msg_type == MessageType.MAP_TILE_REQUEST:
        p: MapTileRequestPayload = payload
        data = {"z": p.z, "x": p.x, "y": p.y, "format": int(p.format)}

    elif msg_type == MessageType.MAP_TILE_RESPONSE:
        p: MapTileResponsePayload = payload
        data = {
            "z": p.z,
            "x": p.x,
            "y": p.y,
            "format": int(p.format),
            "data": p.data,
        }
        if p.error:
            data["error"] = p.error

    elif msg_type == MessageType.MAP_ROUTE_REQUEST:
        p: MapRouteRequestPayload = payload
        data = {
            "start_lat": p.start_lat,
            "start_lon": p.start_lon,
            "end_lat": p.end_lat,
            "end_lon": p.end_lon,
            "mode": int(p.mode),
        }

    elif msg_type == MessageType.MAP_ROUTE_RESPONSE:
        p: MapRouteResponsePayload = payload
        instructions = [
            {"distance_m": i.distance_m, "maneuver": i.maneuver, "street": i.street, "bearing": i.bearing}
            for i in p.instructions
        ]
        data = {
            "points": p.points,
            "instructions": instructions,
            "total_distance_m": p.total_distance_m,
            "total_time_s": p.total_time_s,
        }
        if p.error:
            data["error"] = p.error

    elif msg_type == MessageType.MAP_GEOCODE_REQUEST:
        p: MapGeocodeRequestPayload = payload
        data = {"query": p.query, "max_results": p.max_results}
        if p.bias_lat is not None and p.bias_lon is not None:
            data["bias_lat"] = p.bias_lat
            data["bias_lon"] = p.bias_lon

    elif msg_type == MessageType.MAP_GEOCODE_RESPONSE:
        p: MapGeocodeResponsePayload = payload
        results = [
            {"display_name": r.display_name, "lat": r.lat, "lon": r.lon, "type": r.type}
            for r in p.results
        ]
        data = {"query": p.query, "results": results}
        if p.error:
            data["error"] = p.error

    # Propagation service payloads
    elif msg_type == MessageType.PROP_SYNC_REQUEST:
        p: PropSyncRequestPayload = payload
        data = {
            "lxmf_dest_hash": p.lxmf_dest_hash,
            "known_ids": p.known_ids,
            "max_messages": p.max_messages,
        }

    elif msg_type == MessageType.PROP_SYNC_RESPONSE:
        p: PropSyncResponsePayload = payload
        data = {"count": p.count}
        if p.error:
            data["error"] = p.error

    elif msg_type == MessageType.PROP_MSG_DELIVER:
        p: PropMsgDeliverPayload = payload
        data = {
            "transient_id": p.transient_id,
            "raw_lxmf": p.raw_lxmf,
        }

    elif msg_type == MessageType.PROP_SUBMIT_REQUEST:
        p: PropSubmitRequestPayload = payload
        data = {"raw_lxmf": p.raw_lxmf}

    elif msg_type == MessageType.PROP_SUBMIT_RESPONSE:
        p: PropSubmitResponsePayload = payload
        data = {"accepted": p.accepted}
        if p.transient_id:
            data["transient_id"] = p.transient_id
        if p.error:
            data["error"] = p.error

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
            ai_summary=data.get("ai_summary", False),
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
            summary=data.get("summary"),
        )

    # Maps service payloads
    elif msg_type == MessageType.MAP_TILE_REQUEST:
        return MapTileRequestPayload(
            z=data.get("z", 0),
            x=data.get("x", 0),
            y=data.get("y", 0),
            format=TileFormat(data.get("format", 0)),
        )

    elif msg_type == MessageType.MAP_TILE_RESPONSE:
        return MapTileResponsePayload(
            z=data.get("z", 0),
            x=data.get("x", 0),
            y=data.get("y", 0),
            format=TileFormat(data.get("format", 0)),
            data=data.get("data", b""),
            error=data.get("error"),
        )

    elif msg_type == MessageType.MAP_ROUTE_REQUEST:
        return MapRouteRequestPayload(
            start_lat=data.get("start_lat", 0),
            start_lon=data.get("start_lon", 0),
            end_lat=data.get("end_lat", 0),
            end_lon=data.get("end_lon", 0),
            mode=TravelMode(data.get("mode", 0)),
        )

    elif msg_type == MessageType.MAP_ROUTE_RESPONSE:
        instructions = [
            MapRouteInstruction(
                distance_m=i.get("distance_m", 0),
                maneuver=i.get("maneuver", ""),
                street=i.get("street", ""),
                bearing=i.get("bearing", 0),
            )
            for i in data.get("instructions", [])
        ]
        return MapRouteResponsePayload(
            points=data.get("points", []),
            instructions=instructions,
            total_distance_m=data.get("total_distance_m", 0),
            total_time_s=data.get("total_time_s", 0),
            error=data.get("error"),
        )

    elif msg_type == MessageType.MAP_GEOCODE_REQUEST:
        return MapGeocodeRequestPayload(
            query=data.get("query", ""),
            bias_lat=data.get("bias_lat"),
            bias_lon=data.get("bias_lon"),
            max_results=data.get("max_results", 5),
        )

    elif msg_type == MessageType.MAP_GEOCODE_RESPONSE:
        results = [
            MapGeocodeResult(
                display_name=r.get("display_name", ""),
                lat=r.get("lat", 0),
                lon=r.get("lon", 0),
                type=r.get("type", ""),
            )
            for r in data.get("results", [])
        ]
        return MapGeocodeResponsePayload(
            query=data.get("query", ""),
            results=results,
            error=data.get("error"),
        )

    # Propagation service payloads
    elif msg_type == MessageType.PROP_SYNC_REQUEST:
        return PropSyncRequestPayload(
            lxmf_dest_hash=data.get("lxmf_dest_hash", b""),
            known_ids=data.get("known_ids", []),
            max_messages=data.get("max_messages", 10),
        )

    elif msg_type == MessageType.PROP_SYNC_RESPONSE:
        return PropSyncResponsePayload(
            count=data.get("count", 0),
            error=data.get("error"),
        )

    elif msg_type == MessageType.PROP_MSG_DELIVER:
        return PropMsgDeliverPayload(
            transient_id=data.get("transient_id", b""),
            raw_lxmf=data.get("raw_lxmf", b""),
        )

    elif msg_type == MessageType.PROP_SUBMIT_REQUEST:
        return PropSubmitRequestPayload(
            raw_lxmf=data.get("raw_lxmf", b""),
        )

    elif msg_type == MessageType.PROP_SUBMIT_RESPONSE:
        return PropSubmitResponsePayload(
            accepted=data.get("accepted", False),
            transient_id=data.get("transient_id", b""),
            error=data.get("error"),
        )

    return data
