"""Core Reticulum/LXMF integration for companion server."""

import os
import time
import threading
import logging
from pathlib import Path
from typing import Callable, Optional, Any
from dataclasses import dataclass, field

import RNS
import LXMF

from .config import Config
from .trust_manager import TrustManager
from .protocol import (
    MessageType,
    ServiceMessage,
    encode_service_fields,
    decode_service_fields,
    TrustOfferPayload,
    TrustAcceptPayload,
    NTPRequestPayload,
    NTPResponsePayload,
    SearchRequestPayload,
    SearchResponsePayload,
    MapTileRequestPayload,
    MapRouteRequestPayload,
    MapGeocodeRequestPayload,
    PropSyncRequestPayload,
    PropSyncResponsePayload,
    PropMsgDeliverPayload,
    PropSubmitRequestPayload,
    PropSubmitResponsePayload,
)
from .services import NTPService, SearchService, PropagationService
from .services.maps_service import MapsService

logger = logging.getLogger(__name__)


@dataclass
class AnnounceInfo:
    """Information about a received announce."""

    destination_hash: bytes
    display_name: str
    app_data: bytes
    timestamp: float
    device_type: Optional[str] = None  # "companion-server", "rdeck", or None for unknown
    services: Optional[list[str]] = None  # Services offered (for companion servers)
    identity: Any = None  # The announced RNS.Identity (needed to send messages back)

    @property
    def hash_hex(self) -> str:
        return self.destination_hash.hex()

    @property
    def is_companion_server(self) -> bool:
        return self.device_type == "companion-server"

    @property
    def is_rdeck(self) -> bool:
        return self.device_type == "rdeck"

    @property
    def is_known_type(self) -> bool:
        return self.device_type is not None


@dataclass
class ServiceEvent:
    """Structured event emitted when a service handles a request."""

    service: str  # "ntp", "search", "maps"
    event_type: str  # "request", "response", "error"
    device_name: str
    details: str  # Human-readable summary
    timestamp: float = field(default_factory=time.time)


class AnnounceHandler:
    """Handler for RNS announces.

    RNS.Transport.register_announce_handler requires an object with:
    - aspect_filter: str or None to filter announces by aspect
    - received_announce(destination_hash, announced_identity, app_data): callback method
    """

    def __init__(self, callback: Callable[[bytes, Any, bytes], None], aspect_filter: Optional[str] = None):
        self.aspect_filter = aspect_filter
        self._callback = callback

    def received_announce(self, destination_hash: bytes, announced_identity, app_data: bytes):
        """Called when an announce is received."""
        self._callback(destination_hash, announced_identity, app_data)


class ReticulumService:
    """Manages Reticulum network and LXMF messaging."""

    LXMF_APP_NAME = "lxmf"
    LXMF_ASPECT = "delivery"

    def __init__(self, config: Config, trust_manager: TrustManager):
        self.config = config
        self.trust_manager = trust_manager
        self._running = False
        self._thread: Optional[threading.Thread] = None

        # Callbacks for TUI updates
        self._announce_callbacks: list[Callable[[AnnounceInfo], None]] = []
        self._message_callbacks: list[Callable[[ServiceMessage, bytes], None]] = []
        self._log_callbacks: list[Callable[[str], None]] = []
        self._service_event_callbacks: list[Callable[[ServiceEvent], None]] = []

        # Services
        self._ntp_service = NTPService()
        self._search_service = SearchService(config)
        self._maps_service = MapsService(config) if config.maps_enabled else None
        self._propagation_service: Optional[PropagationService] = None  # Initialized after LXMF router

        # RNS/LXMF objects (initialized in start())
        self._reticulum: Optional[RNS.Reticulum] = None
        self._identity: Optional[RNS.Identity] = None
        self._lxmf_router: Optional[LXMF.LXMRouter] = None
        self._lxmf_destination: Optional[RNS.Destination] = None

        # Announce tracking
        self._announces: dict[str, AnnounceInfo] = {}

    def start(self):
        """Start the Reticulum service.

        Initialization happens in the main thread (required for signal handlers),
        then the event loop runs in a background thread.
        """
        if self._running:
            return

        # Initialize in main thread (RNS.Reticulum needs to set signal handlers)
        try:
            self._initialize_reticulum()
            self._log("Reticulum service started")
        except Exception as e:
            self._log(f"Reticulum initialization error: {e}")
            logger.exception("Reticulum initialization error")
            return

        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        """Stop the Reticulum service."""
        self._running = False
        if self._thread:
            self._thread.join(timeout=5.0)

    def _run(self):
        """Background event loop."""
        try:
            while self._running:
                time.sleep(0.1)

        except Exception as e:
            self._log(f"Reticulum service error: {e}")
            logger.exception("Reticulum service error")

    def _initialize_reticulum(self):
        """Initialize Reticulum, identity, and LXMF.

        Uses the system Reticulum config (~/.reticulum/) to share interfaces
        with other Reticulum applications. Identity is still stored separately
        in the companion server's data directory.
        """
        # Initialize Reticulum using system config (None = default ~/.reticulum/)
        # RNS_CONFIG_DIR env var overrides for Docker deployments
        config_dir = os.environ.get("RNS_CONFIG_DIR", None)
        self._reticulum = RNS.Reticulum(configdir=config_dir)

        # Load or create identity
        identity_path = self.config.identity_path
        if identity_path.exists():
            self._identity = RNS.Identity.from_file(str(identity_path))
            self._log(f"Loaded identity: {self._identity.hash.hex()[:16]}...")
        else:
            self._identity = RNS.Identity()
            self._identity.to_file(str(identity_path))
            self._log(f"Created new identity: {self._identity.hash.hex()[:16]}...")

        # Initialize LXMF router
        self._lxmf_router = LXMF.LXMRouter(
            identity=self._identity,
            storagepath=str(self.config.data_dir / "lxmf"),
        )

        # Enable propagation if configured
        if self.config.propagation_enabled:
            try:
                self._lxmf_router.enable_propagation()
                self._propagation_service = PropagationService(
                    self._lxmf_router, self.config.data_dir
                )
                # Add "propagation" to enabled services for trust offers
                if "propagation" not in self.config.enabled_services:
                    self.config.enabled_services.append("propagation")
                self._log("LXMF propagation node enabled")
            except Exception as e:
                self._log(f"Failed to enable propagation: {e}")
                logger.exception("Failed to enable propagation")

        # Register delivery callback
        self._lxmf_router.register_delivery_callback(self._on_lxmf_delivery)

        # Register our identity for delivery and get the destination
        self._lxmf_destination = self._lxmf_router.register_delivery_identity(
            self._identity,
            display_name=self.config.server_name,
        )
        self._log(f"LXMF destination: {self._lxmf_destination.hash.hex()}")

        # Register announce handler (RNS requires an object with aspect_filter and received_announce)
        self._announce_handler = AnnounceHandler(self._on_announce, aspect_filter="lxmf.delivery")
        RNS.Transport.register_announce_handler(self._announce_handler)

        # Announce ourselves
        self._announce()

    def _announce(self):
        """Announce our presence on the network."""
        if not self._lxmf_router or not self._identity or not self._lxmf_destination:
            return

        # Build custom app_data with device type in extended format:
        # [name_binary, stamp_cost_or_null, device_type]
        import msgpack
        app_data = msgpack.packb([
            self.config.server_name.encode("utf-8"),  # name as binary
            None,  # stamp_cost = null
            "companion-server",  # device_type identifier
        ])

        # Announce with our custom app_data
        self._lxmf_destination.announce(app_data=app_data)
        self._log(f"Announced as '{self.config.server_name}' [companion-server] with services: {self.config.enabled_services}")

    def _on_announce(self, destination_hash: bytes, announced_identity, app_data: bytes):
        """Handle incoming announces."""
        try:
            # Parse app_data - standard LXMF format: [name_binary, stamp_cost_or_null]
            display_name = destination_hash.hex()[:12] + "..."
            device_type = None
            services = None

            if app_data:
                try:
                    import msgpack

                    data = msgpack.unpackb(app_data, raw=False)
                    if isinstance(data, list) and len(data) > 0:
                        # Parse name from position [0]
                        name = data[0]
                        if isinstance(name, bytes):
                            display_name = name.decode("utf-8", errors="replace")
                        elif isinstance(name, str):
                            display_name = name

                        # Parse device_type from position [2] (extended format)
                        # Format: [name_binary, stamp_cost_or_null, device_type]
                        if len(data) > 2 and isinstance(data[2], str):
                            device_type = data[2]
                            if device_type == "companion-server":
                                services = self.config.enabled_services  # Assume same services

                        # Legacy fallback #1: detect device type from display name prefix
                        # [CS] = companion-server, [rD] = rdeck
                        if not device_type:
                            if display_name.startswith("[CS] "):
                                device_type = "companion-server"
                                display_name = display_name[5:]  # Strip prefix for display
                                services = self.config.enabled_services
                            elif display_name.startswith("[rD] "):
                                device_type = "rdeck"
                                display_name = display_name[5:]  # Strip prefix for display

                        # Legacy fallback #2: metadata dict in position [1]
                        if not device_type and len(data) > 1 and isinstance(data[1], dict):
                            metadata = data[1]
                            device_type = metadata.get("type")
                            if not services:
                                services = metadata.get("services")
                except Exception:
                    pass

            announce_info = AnnounceInfo(
                destination_hash=destination_hash,
                display_name=display_name,
                app_data=app_data,
                timestamp=time.time(),
                device_type=device_type,
                services=services,
                identity=announced_identity,  # Store identity for sending messages back
            )

            # Store announce
            self._announces[announce_info.hash_hex] = announce_info

            # Notify callbacks
            for callback in self._announce_callbacks:
                try:
                    callback(announce_info)
                except Exception as e:
                    logger.exception(f"Announce callback error: {e}")

            # Log with type info if available
            type_info = f" [{device_type}]" if device_type else ""
            self._log(f"Received announce from '{display_name}'{type_info} ({announce_info.hash_hex[:12]}...)")

        except Exception as e:
            logger.exception(f"Error handling announce: {e}")

    def _on_lxmf_delivery(self, message: LXMF.LXMessage):
        """Handle incoming LXMF messages."""
        try:
            source_hash = message.source_hash
            fields = message.fields or {}

            self._log(f"Received message from {source_hash.hex()[:12]}...")

            # Check if this is a service message
            if "msg_type" in fields:
                service_msg = decode_service_fields(fields)
                self._handle_service_message(service_msg, source_hash)

                # Notify callbacks
                for callback in self._message_callbacks:
                    try:
                        callback(service_msg, source_hash)
                    except Exception as e:
                        logger.exception(f"Message callback error: {e}")
            else:
                self._log(f"Received non-service message: {message.content}")

        except Exception as e:
            logger.exception(f"Error handling LXMF delivery: {e}")

    def _handle_service_message(self, msg: ServiceMessage, source_hash: bytes):
        """Handle a service message based on type."""
        hash_hex = source_hash.hex()

        if msg.msg_type == MessageType.TRUST_ACCEPT:
            # Device accepted our trust offer
            payload: TrustAcceptPayload = msg.payload
            self.trust_manager.accept_trust(hash_hex, payload.device_name)
            self._log(f"Trust accepted by '{payload.device_name}'")

        elif msg.msg_type == MessageType.TRUST_REVOKE:
            # Trust revoked
            self.trust_manager.revoke_trust(hash_hex)
            self._log(f"Trust revoked by {hash_hex[:12]}...")

        elif msg.msg_type == MessageType.NTP_REQUEST:
            # NTP time request
            device = self.trust_manager.get_device(hash_hex)
            device_name = device.name if device else hash_hex[:12] + "..."

            # If we offered trust and they're sending requests, they've accepted
            # Upgrade to mutual trust (handles case where TRUST_ACCEPT was lost)
            if self.trust_manager.is_trust_pending(hash_hex):
                self._log(f"[NTP] Device '{device_name}' sending request - upgrading to mutual trust")
                self.trust_manager.accept_trust(hash_hex)

            if not self.trust_manager.is_mutually_trusted(hash_hex):
                self._log(f"[NTP] Rejected request from untrusted '{device_name}'")
                return

            payload: NTPRequestPayload = msg.payload
            response = self._ntp_service.handle_request(payload)
            self._send_service_message(
                source_hash,
                ServiceMessage(
                    msg_type=MessageType.NTP_RESPONSE,
                    service="ntp",
                    payload=response,
                    request_id=msg.request_id,
                ),
            )
            from datetime import datetime
            time_str = datetime.fromtimestamp(response.server_timestamp).strftime("%Y-%m-%d %H:%M:%S")
            self._log(f"[NTP] Sent time to '{device_name}': {time_str}")
            self._fire_service_event("ntp", "response", device_name, f"Sent time: {time_str}")

        elif msg.msg_type == MessageType.SEARCH_REQUEST:
            # Search request
            device = self.trust_manager.get_device(hash_hex)
            device_name = device.name if device else hash_hex[:12] + "..."

            # If we offered trust and they're sending requests, they've accepted
            # Upgrade to mutual trust (handles case where TRUST_ACCEPT was lost)
            if self.trust_manager.is_trust_pending(hash_hex):
                self._log(f"[Search] Device '{device_name}' sending request - upgrading to mutual trust")
                self.trust_manager.accept_trust(hash_hex)

            if not self.trust_manager.is_mutually_trusted(hash_hex):
                self._log(f"[Search] Rejected request from untrusted '{device_name}'")
                return

            payload: SearchRequestPayload = msg.payload
            self._log(f"[Search] '{device_name}' searching: '{payload.query}'")
            response = self._search_service.handle_request(payload)
            self._send_service_message(
                source_hash,
                ServiceMessage(
                    msg_type=MessageType.SEARCH_RESPONSE,
                    service="search",
                    payload=response,
                    request_id=msg.request_id,
                ),
            )
            if response.error:
                self._log(f"[Search] Error for '{device_name}': {response.error}")
                self._fire_service_event("search", "error", device_name, f"Query: '{payload.query}' - {response.error}")
            else:
                self._log(f"[Search] Sent {len(response.results)} results to '{device_name}'")
                self._fire_service_event("search", "response", device_name, f"'{payload.query}' -> {len(response.results)} results")

        elif msg.msg_type in (MessageType.MAP_TILE_REQUEST, MessageType.MAP_ROUTE_REQUEST, MessageType.MAP_GEOCODE_REQUEST):
            # Maps service requests
            if not self._maps_service:
                self._log(f"[Maps] Service not enabled, ignoring request")
                return

            device = self.trust_manager.get_device(hash_hex)
            device_name = device.name if device else hash_hex[:12] + "..."

            # Upgrade to mutual trust if pending
            if self.trust_manager.is_trust_pending(hash_hex):
                self._log(f"[Maps] Device '{device_name}' sending request - upgrading to mutual trust")
                self.trust_manager.accept_trust(hash_hex)

            if not self.trust_manager.is_mutually_trusted(hash_hex):
                self._log(f"[Maps] Rejected request from untrusted '{device_name}'")
                return

            if msg.msg_type == MessageType.MAP_TILE_REQUEST:
                payload: MapTileRequestPayload = msg.payload
                self._log(f"[Maps] '{device_name}' requesting tile z={payload.z} x={payload.x} y={payload.y}")
                response = self._maps_service._handle_tile_request(payload)
                self._send_service_message(
                    source_hash,
                    ServiceMessage(
                        msg_type=MessageType.MAP_TILE_RESPONSE,
                        service="maps",
                        payload=response,
                        request_id=msg.request_id,
                    ),
                )
                if response.error:
                    self._log(f"[Maps] Tile error for '{device_name}': {response.error}")
                    self._fire_service_event("maps", "error", device_name, f"Tile z={payload.z} x={payload.x} y={payload.y} - {response.error}")
                else:
                    self._log(f"[Maps] Sent tile to '{device_name}'")
                    self._fire_service_event("maps", "response", device_name, f"Tile z={payload.z} x={payload.x} y={payload.y}")

            elif msg.msg_type == MessageType.MAP_ROUTE_REQUEST:
                payload: MapRouteRequestPayload = msg.payload
                self._log(f"[Maps] '{device_name}' requesting route")
                response = self._maps_service._handle_route_request(payload)
                self._send_service_message(
                    source_hash,
                    ServiceMessage(
                        msg_type=MessageType.MAP_ROUTE_RESPONSE,
                        service="maps",
                        payload=response,
                        request_id=msg.request_id,
                    ),
                )
                if response.error:
                    self._log(f"[Maps] Route error for '{device_name}': {response.error}")
                    self._fire_service_event("maps", "error", device_name, f"Route - {response.error}")
                else:
                    self._log(f"[Maps] Sent route ({len(response.points)//2} points) to '{device_name}'")
                    self._fire_service_event("maps", "response", device_name, f"Route: {len(response.points)//2} points")

            elif msg.msg_type == MessageType.MAP_GEOCODE_REQUEST:
                payload: MapGeocodeRequestPayload = msg.payload
                self._log(f"[Maps] '{device_name}' geocoding: '{payload.query}'")
                response = self._maps_service._handle_geocode_request(payload)
                self._send_service_message(
                    source_hash,
                    ServiceMessage(
                        msg_type=MessageType.MAP_GEOCODE_RESPONSE,
                        service="maps",
                        payload=response,
                        request_id=msg.request_id,
                    ),
                )
                if response.error:
                    self._log(f"[Maps] Geocode error for '{device_name}': {response.error}")
                    self._fire_service_event("maps", "error", device_name, f"Geocode '{payload.query}' - {response.error}")
                else:
                    self._log(f"[Maps] Sent {len(response.results)} geocode results to '{device_name}'")
                    self._fire_service_event("maps", "response", device_name, f"Geocode '{payload.query}' -> {len(response.results)} results")

        elif msg.msg_type in (MessageType.PROP_SYNC_REQUEST, MessageType.PROP_SUBMIT_REQUEST):
            # Propagation service requests
            if not self._propagation_service:
                self._log(f"[Propagation] Service not enabled, ignoring request")
                return

            device = self.trust_manager.get_device(hash_hex)
            device_name = device.name if device else hash_hex[:12] + "..."

            # Upgrade to mutual trust if pending
            if self.trust_manager.is_trust_pending(hash_hex):
                self._log(f"[Propagation] Device '{device_name}' sending request - upgrading to mutual trust")
                self.trust_manager.accept_trust(hash_hex)

            if not self.trust_manager.is_mutually_trusted(hash_hex):
                self._log(f"[Propagation] Rejected request from untrusted '{device_name}'")
                return

            if msg.msg_type == MessageType.PROP_SYNC_REQUEST:
                payload: PropSyncRequestPayload = msg.payload
                self._log(f"[Propagation] '{device_name}' requesting sync")
                sync_response, messages = self._propagation_service.handle_sync_request(payload, hash_hex)

                # Send sync response first
                self._send_service_message(
                    source_hash,
                    ServiceMessage(
                        msg_type=MessageType.PROP_SYNC_RESPONSE,
                        service="propagation",
                        payload=sync_response,
                        request_id=msg.request_id,
                    ),
                )

                # Then send individual messages
                delivered_ids = []
                for deliver_msg in messages:
                    self._send_service_message(
                        source_hash,
                        ServiceMessage(
                            msg_type=MessageType.PROP_MSG_DELIVER,
                            service="propagation",
                            payload=deliver_msg,
                            request_id=msg.request_id,
                        ),
                    )
                    delivered_ids.append(deliver_msg.transient_id)

                # Track delivered messages
                if delivered_ids:
                    self._propagation_service.mark_delivered(hash_hex, delivered_ids)

                self._log(f"[Propagation] Sent {len(messages)} messages to '{device_name}'")
                self._fire_service_event("propagation", "response", device_name, f"Sync: {len(messages)} messages delivered")

            elif msg.msg_type == MessageType.PROP_SUBMIT_REQUEST:
                payload: PropSubmitRequestPayload = msg.payload
                self._log(f"[Propagation] '{device_name}' submitting message for propagation")
                response = self._propagation_service.handle_submit_request(payload)
                self._send_service_message(
                    source_hash,
                    ServiceMessage(
                        msg_type=MessageType.PROP_SUBMIT_RESPONSE,
                        service="propagation",
                        payload=response,
                        request_id=msg.request_id,
                    ),
                )
                if response.accepted:
                    self._log(f"[Propagation] Accepted message from '{device_name}': {response.transient_id.hex()[:16]}...")
                    self._fire_service_event("propagation", "response", device_name, "Message accepted for propagation")
                else:
                    self._log(f"[Propagation] Rejected message from '{device_name}': {response.error}")
                    self._fire_service_event("propagation", "error", device_name, f"Submit rejected: {response.error}")

    def send_trust_offer(self, destination_hash: str):
        """Send a trust offer to a device."""
        try:
            dest_bytes = bytes.fromhex(destination_hash)

            payload = TrustOfferPayload(
                server_name=self.config.server_name,
                services=self.config.enabled_services,
            )

            msg = ServiceMessage(
                msg_type=MessageType.TRUST_OFFER,
                service="trust",
                payload=payload,
                request_id=int(time.time() * 1000) & 0xFFFFFFFF,
            )

            self._send_service_message(dest_bytes, msg)

            # Mark as pending in trust manager
            display_name = self._announces.get(destination_hash, AnnounceInfo(
                destination_hash=dest_bytes,
                display_name=destination_hash[:12] + "...",
                app_data=b"",
                timestamp=time.time(),
            )).display_name
            self.trust_manager.offer_trust(destination_hash, display_name)

            self._log(f"Sent trust offer to {destination_hash[:12]}...")

        except Exception as e:
            self._log(f"Error sending trust offer: {e}")
            logger.exception("Error sending trust offer")

    def _send_service_message(self, destination_hash: bytes, msg: ServiceMessage):
        """Send a service message via LXMF."""
        if not self._lxmf_router:
            self._log("Cannot send: LXMF router not initialized")
            return

        try:
            # Create LXMF message with service fields
            fields = encode_service_fields(msg)
            hash_hex = destination_hash.hex()

            self._log(f"Creating LXMF message to {hash_hex[:12]}...")

            # Try to get the identity from our announce cache or recall from RNS
            announce_info = self._announces.get(hash_hex)
            identity = None
            if announce_info and announce_info.identity:
                identity = announce_info.identity
                self._log(f"Using cached identity from announce")
            else:
                # Try to recall from RNS identity cache
                identity = RNS.Identity.recall(destination_hash)
                if identity:
                    self._log(f"Recalled identity from RNS cache")
                else:
                    self._log(f"WARNING: No identity found for {hash_hex[:12]}...")

            # Create destination object if we have identity, otherwise fall back to hash
            if identity:
                destination = RNS.Destination(
                    identity,
                    RNS.Destination.OUT,
                    RNS.Destination.SINGLE,
                    "lxmf",
                    "delivery"
                )
                self._log(f"Created destination object from identity")

                lxm = LXMF.LXMessage(
                    destination,
                    self._lxmf_destination,
                    "",  # content - Service messages use fields, not content
                    fields=fields,
                    desired_method=LXMF.LXMessage.DIRECT,  # Use link-based delivery
                )
            else:
                # Fallback to hash-based creation (may not work without identity)
                self._log(f"Falling back to destination_hash (may fail)")
                lxm = LXMF.LXMessage(
                    destination_hash=destination_hash,
                    source_hash=self._lxmf_destination.hash,
                    content="",
                    fields=fields,
                    desired_method=LXMF.LXMessage.DIRECT,
                )

            # Track delivery status
            def on_delivered(message):
                self._log(f"Message DELIVERED to {hash_hex[:12]}!")

            def on_failed(message):
                state_names = {
                    LXMF.LXMessage.GENERATING: "GENERATING",
                    LXMF.LXMessage.OUTBOUND: "OUTBOUND",
                    LXMF.LXMessage.SENDING: "SENDING",
                    LXMF.LXMessage.SENT: "SENT",
                    LXMF.LXMessage.DELIVERED: "DELIVERED",
                    LXMF.LXMessage.FAILED: "FAILED",
                }
                state_name = state_names.get(message.state, f"UNKNOWN({message.state})")
                self._log(f"Message FAILED to {hash_hex[:12]}... (state: {state_name})")

            lxm.register_delivery_callback(on_delivered)
            lxm.register_failed_callback(on_failed)

            self._log(f"Sending LXMF message (method: DIRECT)...")
            self._lxmf_router.handle_outbound(lxm)

            # Log detailed state info
            state_names = {
                LXMF.LXMessage.GENERATING: "GENERATING",
                LXMF.LXMessage.OUTBOUND: "OUTBOUND",
                LXMF.LXMessage.SENDING: "SENDING",
                LXMF.LXMessage.SENT: "SENT",
                LXMF.LXMessage.DELIVERED: "DELIVERED",
                LXMF.LXMessage.FAILED: "FAILED",
            }
            state_name = state_names.get(lxm.state, f"UNKNOWN({lxm.state})")
            self._log(f"Message queued (state: {state_name}, method: {lxm.method})")

        except Exception as e:
            self._log(f"Error sending service message: {e}")
            logger.exception(f"Error sending service message: {e}")

    def _fire_service_event(self, service: str, event_type: str, device_name: str, details: str):
        """Fire a structured service event to registered callbacks."""
        event = ServiceEvent(
            service=service,
            event_type=event_type,
            device_name=device_name,
            details=details,
        )
        for callback in self._service_event_callbacks:
            try:
                callback(event)
            except Exception as e:
                logger.exception(f"Service event callback error: {e}")

    def _log(self, message: str):
        """Log a message and notify callbacks."""
        logger.info(message)
        for callback in self._log_callbacks:
            try:
                callback(message)
            except Exception:
                pass

    # Callback registration

    def on_announce(self, callback: Callable[[AnnounceInfo], None]):
        """Register a callback for announces."""
        self._announce_callbacks.append(callback)

    def on_message(self, callback: Callable[[ServiceMessage, bytes], None]):
        """Register a callback for service messages."""
        self._message_callbacks.append(callback)

    def on_log(self, callback: Callable[[str], None]):
        """Register a callback for log messages."""
        self._log_callbacks.append(callback)

    def on_service_event(self, callback: Callable[[ServiceEvent], None]):
        """Register a callback for structured service events."""
        self._service_event_callbacks.append(callback)

    # Properties

    @property
    def announces(self) -> dict[str, AnnounceInfo]:
        """Get all received announces."""
        return self._announces.copy()

    @property
    def identity_hash(self) -> Optional[str]:
        """Get our identity hash."""
        if self._identity:
            return self._identity.hash.hex()
        return None

    @property
    def destination_hash(self) -> Optional[str]:
        """Get our LXMF destination hash."""
        if self._lxmf_destination:
            return self._lxmf_destination.hash.hex()
        return None
