"""Core Reticulum/LXMF integration for companion server."""

import time
import threading
import logging
from pathlib import Path
from typing import Callable, Optional, Any
from dataclasses import dataclass

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
)
from .services import NTPService, SearchService

logger = logging.getLogger(__name__)


@dataclass
class AnnounceInfo:
    """Information about a received announce."""

    destination_hash: bytes
    display_name: str
    app_data: bytes
    timestamp: float

    @property
    def hash_hex(self) -> str:
        return self.destination_hash.hex()


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

        # Services
        self._ntp_service = NTPService()
        self._search_service = SearchService(config)

        # RNS/LXMF objects (initialized in start())
        self._reticulum: Optional[RNS.Reticulum] = None
        self._identity: Optional[RNS.Identity] = None
        self._lxmf_router: Optional[LXMF.LXMRouter] = None
        self._lxmf_destination: Optional[RNS.Destination] = None

        # Announce tracking
        self._announces: dict[str, AnnounceInfo] = {}

    def start(self):
        """Start the Reticulum service in a background thread."""
        if self._running:
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
        """Main service loop."""
        try:
            self._initialize_reticulum()
            self._log("Reticulum service started")

            while self._running:
                time.sleep(0.1)

        except Exception as e:
            self._log(f"Reticulum service error: {e}")
            logger.exception("Reticulum service error")

    def _initialize_reticulum(self):
        """Initialize Reticulum, identity, and LXMF."""
        # Initialize Reticulum
        self._reticulum = RNS.Reticulum(
            configdir=str(self.config.data_dir / "reticulum")
        )

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

        # Register delivery callback
        self._lxmf_router.register_delivery_callback(self._on_lxmf_delivery)

        # Get our LXMF destination
        self._lxmf_destination = self._lxmf_router.get_delivery_destination()
        self._log(f"LXMF destination: {self._lxmf_destination.hash.hex()}")

        # Register announce handler
        RNS.Transport.register_announce_handler(self._on_announce)

        # Announce ourselves
        self._announce()

    def _announce(self):
        """Announce our presence on the network."""
        if not self._lxmf_destination:
            return

        # Build app_data like rDeck does: [name_binary, null]
        import msgpack

        app_data = msgpack.packb([self.config.server_name.encode(), None])
        self._lxmf_destination.announce(app_data=app_data)
        self._log(f"Announced as '{self.config.server_name}'")

    def _on_announce(self, destination_hash: bytes, announced_identity, app_data: bytes):
        """Handle incoming announces."""
        try:
            # Parse display name from app_data (msgpack: [name_binary, null])
            display_name = destination_hash.hex()[:12] + "..."
            if app_data:
                try:
                    import msgpack

                    data = msgpack.unpackb(app_data, raw=False)
                    if isinstance(data, list) and len(data) > 0:
                        name = data[0]
                        if isinstance(name, bytes):
                            display_name = name.decode("utf-8", errors="replace")
                        elif isinstance(name, str):
                            display_name = name
                except Exception:
                    pass

            announce_info = AnnounceInfo(
                destination_hash=destination_hash,
                display_name=display_name,
                app_data=app_data,
                timestamp=time.time(),
            )

            # Store announce
            self._announces[announce_info.hash_hex] = announce_info

            # Notify callbacks
            for callback in self._announce_callbacks:
                try:
                    callback(announce_info)
                except Exception as e:
                    logger.exception(f"Announce callback error: {e}")

            self._log(f"Received announce from '{display_name}' ({announce_info.hash_hex[:12]}...)")

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
            if not self.trust_manager.is_mutually_trusted(hash_hex):
                self._log(f"Rejected NTP request from untrusted device {hash_hex[:12]}...")
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
            self._log(f"Sent NTP response to {hash_hex[:12]}...")

        elif msg.msg_type == MessageType.SEARCH_REQUEST:
            # Search request
            if not self.trust_manager.is_mutually_trusted(hash_hex):
                self._log(f"Rejected search request from untrusted device {hash_hex[:12]}...")
                return

            payload: SearchRequestPayload = msg.payload
            self._log(f"Processing search: '{payload.query}'")
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
            self._log(f"Sent search response ({len(response.results)} results)")

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
            return

        try:
            # Create LXMF message with service fields
            fields = encode_service_fields(msg)

            lxm = LXMF.LXMessage(
                destination_hash=destination_hash,
                source_hash=self._lxmf_destination.hash,
                content="",  # Service messages use fields, not content
                fields=fields,
            )

            self._lxmf_router.handle_outbound(lxm)

        except Exception as e:
            logger.exception(f"Error sending service message: {e}")

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
