"""Mock Reticulum and LXMF implementations for testing.

This module provides mock implementations of RNS and LXMF that allow
testing the companion server without actual network connectivity.

Usage:
    from tests.mocks import create_test_harness

    harness = create_test_harness()
    harness.install()

    # Create and test your ReticulumService
    service = ReticulumService(config, trust_manager)
    service.start()

    # Simulate an announce from a device
    harness.simulate_announce(device_hash, "DeviceName")

    # Simulate receiving an LXMF message
    harness.simulate_message(device_hash, fields)

    # Check what messages were sent
    assert len(harness.sent_messages) == 1

    harness.uninstall()
"""

import os
import time
import threading
from dataclasses import dataclass, field
from typing import Callable, Optional, Any
from pathlib import Path
from unittest.mock import MagicMock


# Global state for the mock transport layer
_mock_transport: Optional["MockTransport"] = None
_original_rns = None
_original_lxmf = None


class MockIdentity:
    """Mock RNS.Identity."""

    def __init__(self, hash_bytes: Optional[bytes] = None):
        if hash_bytes is None:
            # Generate a random-ish hash for testing
            hash_bytes = os.urandom(16)
        self._hash = hash_bytes

    @property
    def hash(self) -> bytes:
        return self._hash

    @classmethod
    def from_file(cls, path: str) -> "MockIdentity":
        """Load identity from file (mock just creates consistent identity)."""
        # Use path hash for consistent identity per path
        path_hash = hash(path).to_bytes(16, byteorder="big", signed=True)
        return cls(path_hash)

    def to_file(self, path: str):
        """Save identity to file (mock is no-op)."""
        # Ensure parent directory exists
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        Path(path).write_bytes(self._hash)

    @classmethod
    def recall(cls, destination_hash: bytes) -> Optional["MockIdentity"]:
        """Recall an identity from cache. Mock returns None (no cache)."""
        return None


class MockDestination:
    """Mock RNS.Destination."""

    # Direction constants
    IN = 0x01
    OUT = 0x02

    # Type constants
    SINGLE = 0x00
    GROUP = 0x01
    PLAIN = 0x02
    LINK = 0x03

    def __init__(self, identity: MockIdentity, direction: int, dest_type: int = None, app_name: str = None, aspect: str = None):
        self._identity = identity
        self._direction = direction
        self._dest_type = dest_type if dest_type is not None else self.SINGLE
        self._app_name = app_name or ""
        self._aspect = aspect or ""
        # For mocking purposes, use the identity hash directly as the destination hash.
        # This allows tests to simulate announces with a known destination hash and
        # verify that messages are sent back to that same hash.
        self._hash = identity.hash[:16]
        self._announced_app_data: Optional[bytes] = None

    @property
    def hash(self) -> bytes:
        return self._hash

    def announce(self, app_data: Optional[bytes] = None):
        """Announce this destination."""
        self._announced_app_data = app_data
        if _mock_transport:
            _mock_transport._record_announce(self._hash, app_data)


class MockTransport:
    """Mock RNS.Transport - central coordinator for the mock network."""

    def __init__(self):
        self._announce_handlers: list[Any] = []  # Handler objects with received_announce method
        self._announces_sent: list[tuple[bytes, Optional[bytes]]] = []
        self._lock = threading.Lock()

    def register_announce_handler(self, handler: Any):
        """Register a handler for incoming announces.

        Handler must be an object with:
        - aspect_filter: str or None
        - received_announce(destination_hash, announced_identity, app_data): method
        """
        with self._lock:
            self._announce_handlers.append(handler)

    def _record_announce(self, dest_hash: bytes, app_data: Optional[bytes]):
        """Record an outgoing announce."""
        with self._lock:
            self._announces_sent.append((dest_hash, app_data))

    def simulate_announce(
        self,
        destination_hash: bytes,
        display_name: str,
        app_data: Optional[bytes] = None,
    ):
        """Simulate receiving an announce from another device."""
        import msgpack

        if app_data is None:
            # Build app_data in rDeck format: [name_binary, null]
            app_data = msgpack.packb([display_name.encode(), None])

        # Create a mock identity for the announced device
        announced_identity = MockIdentity(destination_hash)

        with self._lock:
            handlers = list(self._announce_handlers)

        for handler in handlers:
            try:
                # Check aspect filter if present
                aspect_filter = getattr(handler, "aspect_filter", None)
                # For now, we don't filter - just call the handler
                # In real RNS, it would match against destination aspects

                # Call the handler's received_announce method
                if hasattr(handler, "received_announce"):
                    handler.received_announce(destination_hash, announced_identity, app_data)
                else:
                    # Fallback for plain callables (backwards compat)
                    handler(destination_hash, announced_identity, app_data)
            except Exception as e:
                print(f"Announce handler error: {e}")

    @property
    def announces_sent(self) -> list[tuple[bytes, Optional[bytes]]]:
        """Get list of (dest_hash, app_data) for all announces sent."""
        with self._lock:
            return list(self._announces_sent)


class MockReticulum:
    """Mock RNS.Reticulum."""

    def __init__(self, configdir: Optional[str] = None):
        self.configdir = configdir
        if configdir:
            Path(configdir).mkdir(parents=True, exist_ok=True)


class MockLXMessage:
    """Mock LXMF.LXMessage."""

    # LXMF state constants
    GENERATING = 0
    OUTBOUND = 1
    SENDING = 2
    SENT = 3
    DELIVERED = 4
    FAILED = 5
    DRAFT = 0  # Alias

    # Delivery method constants
    OPPORTUNISTIC = 0x01
    DIRECT = 0x02
    PROPAGATED = 0x03

    def __init__(
        self,
        destination=None,  # Can be MockDestination or bytes (destination_hash)
        source=None,  # Can be MockDestination or bytes (source_hash)
        content: str = "",
        fields: Optional[dict] = None,
        title: str = "",
        desired_method: int = None,
        # Also support kwargs for backwards compat
        destination_hash: bytes = None,
        source_hash: bytes = None,
    ):
        # Handle destination - can be Destination object or hash bytes
        if destination is not None:
            if isinstance(destination, MockDestination):
                self.destination_hash = destination.hash
                self.destination = destination
            else:
                self.destination_hash = destination
                self.destination = None
        elif destination_hash is not None:
            self.destination_hash = destination_hash
            self.destination = None
        else:
            self.destination_hash = b""
            self.destination = None

        # Handle source - can be Destination object or hash bytes
        if source is not None:
            if isinstance(source, MockDestination):
                self.source_hash = source.hash
                self.source = source
            else:
                self.source_hash = source
                self.source = None
        elif source_hash is not None:
            self.source_hash = source_hash
            self.source = None
        else:
            self.source_hash = b""
            self.source = None

        self.content = content
        self.fields = fields or {}
        self.title = title
        self.timestamp = time.time()
        self.desired_method = desired_method or self.DIRECT
        self.method = self.desired_method
        self.state = self.OUTBOUND
        self._delivery_callback = None
        self._failed_callback = None

    def register_delivery_callback(self, callback):
        """Register callback for successful delivery."""
        self._delivery_callback = callback

    def register_failed_callback(self, callback):
        """Register callback for failed delivery."""
        self._failed_callback = callback


class MockLXMRouter:
    """Mock LXMF.LXMRouter."""

    def __init__(self, identity: MockIdentity, storagepath: Optional[str] = None, **kwargs):
        self._identity = identity
        self._storagepath = storagepath
        self._delivery_callback: Optional[Callable[[MockLXMessage], None]] = None
        self._destinations: dict[bytes, MockDestination] = {}
        self._display_names: dict[bytes, str] = {}  # Track display names for announces
        self._stamp_costs: dict[bytes, Optional[int]] = {}  # Track stamp costs
        self._outbound_messages: list[MockLXMessage] = []
        self._announces_sent: list[tuple[bytes, bytes]] = []  # (dest_hash, app_data)
        self._lock = threading.Lock()

        if storagepath:
            Path(storagepath).mkdir(parents=True, exist_ok=True)

    def register_delivery_callback(self, callback: Callable[[MockLXMessage], None]):
        """Register callback for incoming message delivery."""
        self._delivery_callback = callback

    def register_delivery_identity(
        self,
        identity: MockIdentity,
        display_name: Optional[str] = None,
        stamp_cost: Optional[int] = None,
    ) -> MockDestination:
        """Register an identity for delivery and return its destination."""
        dest = MockDestination(identity, MockDestination.IN, MockDestination.SINGLE, "lxmf", "delivery")
        self._destinations[identity.hash] = dest
        self._display_names[identity.hash] = display_name
        self._stamp_costs[identity.hash] = stamp_cost
        return dest

    def get_announce_app_data(self, destination_hash: bytes) -> bytes:
        """Build announce app_data in standard LXMF format."""
        import msgpack
        display_name = self._display_names.get(destination_hash)
        stamp_cost = self._stamp_costs.get(destination_hash)

        name_bytes = display_name.encode() if display_name else None
        return msgpack.packb([name_bytes, stamp_cost])

    def announce(self, destination_hash: bytes, attached_interface=None):
        """Announce a delivery destination."""
        if destination_hash in self._destinations:
            app_data = self.get_announce_app_data(destination_hash)
            self._destinations[destination_hash].announce(app_data=app_data)
            with self._lock:
                self._announces_sent.append((destination_hash, app_data))

    def get_delivery_destination(self) -> Optional[MockDestination]:
        """Get the first registered delivery destination (for backwards compat)."""
        if self._destinations:
            return next(iter(self._destinations.values()))
        return None

    def handle_outbound(self, message: MockLXMessage):
        """Handle an outbound message."""
        with self._lock:
            self._outbound_messages.append(message)

    def simulate_delivery(self, message: MockLXMessage):
        """Simulate delivery of an incoming message."""
        if self._delivery_callback:
            self._delivery_callback(message)

    @property
    def outbound_messages(self) -> list[MockLXMessage]:
        """Get list of messages sent via handle_outbound."""
        with self._lock:
            return list(self._outbound_messages)

    def clear_outbound(self):
        """Clear the outbound message list."""
        with self._lock:
            self._outbound_messages.clear()

    # Propagation support
    def enable_propagation(self):
        """Enable propagation node (mock - just sets a flag)."""
        self._propagation_enabled = True

    @property
    def propagation_entries(self) -> dict:
        """Propagation store entries: {transient_id: (dest_hash, filepath)}."""
        if not hasattr(self, '_propagation_entries'):
            self._propagation_entries = {}
        return self._propagation_entries

    def lxmf_propagation(self, raw_bytes, stamp_data=b'', stamp_value=0, is_paper_message=False):
        """Inject raw LXMF bytes into the propagation store (mock)."""
        import hashlib
        transient_id = hashlib.sha256(raw_bytes).digest()
        if not hasattr(self, '_propagation_store'):
            self._propagation_store = []
        self._propagation_store.append({
            'transient_id': transient_id,
            'raw_bytes': raw_bytes,
            'stamp_data': stamp_data,
            'stamp_value': stamp_value,
        })


@dataclass
class ReticulumTestHarness:
    """Test harness for Reticulum service testing.

    Provides a clean interface for:
    - Installing/uninstalling mocks
    - Simulating network events
    - Inspecting sent messages
    """

    transport: MockTransport = field(default_factory=MockTransport)
    routers: list[MockLXMRouter] = field(default_factory=list)
    _installed: bool = False

    def install(self):
        """Install the mock RNS and LXMF modules."""
        if self._installed:
            return

        install_mocks(self.transport)
        self._installed = True

    def uninstall(self):
        """Restore original RNS and LXMF modules."""
        if not self._installed:
            return

        uninstall_mocks()
        self._installed = False

    def register_router(self, router: MockLXMRouter):
        """Register a router for message simulation."""
        self.routers.append(router)

    def simulate_announce(
        self,
        destination_hash: bytes,
        display_name: str,
        app_data: Optional[bytes] = None,
    ):
        """Simulate receiving an announce from a device."""
        self.transport.simulate_announce(destination_hash, display_name, app_data)

    def simulate_message(
        self,
        source_hash: bytes,
        fields: dict,
        content: str = "",
        router_index: int = 0,
    ):
        """Simulate receiving an LXMF message.

        Args:
            source_hash: The sender's hash
            fields: LXMF fields dict (msg_type, service, payload, request_id)
            content: Optional message content
            router_index: Which router to deliver to (default: first registered)
        """
        if not self.routers:
            raise RuntimeError("No routers registered. Service may not be started.")

        router = self.routers[router_index]
        dest_hash = router.get_delivery_destination().hash

        message = MockLXMessage(
            destination_hash=dest_hash,
            source_hash=source_hash,
            content=content,
            fields=fields,
        )

        router.simulate_delivery(message)

    @property
    def sent_messages(self) -> list[MockLXMessage]:
        """Get all messages sent by all registered routers."""
        messages = []
        for router in self.routers:
            messages.extend(router.outbound_messages)
        return messages

    @property
    def announces_sent(self) -> list[tuple[bytes, Optional[bytes]]]:
        """Get all announces sent."""
        return self.transport.announces_sent

    def clear_sent(self):
        """Clear all sent messages and announces."""
        for router in self.routers:
            router.clear_outbound()
        self.transport._announces_sent.clear()

    def __enter__(self):
        self.install()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.uninstall()
        return False


def install_mocks(transport: Optional[MockTransport] = None):
    """Install mock RNS and LXMF modules.

    Args:
        transport: Optional MockTransport to use. Creates new one if not provided.
    """
    global _mock_transport, _original_rns, _original_lxmf
    import sys

    _mock_transport = transport or MockTransport()

    # Save originals if present
    _original_rns = sys.modules.get("RNS")
    _original_lxmf = sys.modules.get("LXMF")

    # Create mock modules
    mock_rns = MagicMock()
    mock_rns.Reticulum = MockReticulum
    mock_rns.Identity = MockIdentity
    mock_rns.Destination = MockDestination
    mock_rns.Transport = _mock_transport

    mock_lxmf = MagicMock()
    mock_lxmf.LXMRouter = MockLXMRouter
    mock_lxmf.LXMessage = MockLXMessage

    # Install mocks
    sys.modules["RNS"] = mock_rns
    sys.modules["LXMF"] = mock_lxmf

    # Force reimport of reticulum_service to pick up mocks
    if "companion_server.reticulum_service" in sys.modules:
        del sys.modules["companion_server.reticulum_service"]


def uninstall_mocks():
    """Restore original RNS and LXMF modules."""
    global _mock_transport, _original_rns, _original_lxmf
    import sys

    _mock_transport = None

    if _original_rns is not None:
        sys.modules["RNS"] = _original_rns
    else:
        sys.modules.pop("RNS", None)

    if _original_lxmf is not None:
        sys.modules["LXMF"] = _original_lxmf
    else:
        sys.modules.pop("LXMF", None)

    _original_rns = None
    _original_lxmf = None


def get_mock_transport() -> Optional[MockTransport]:
    """Get the current mock transport, if installed."""
    return _mock_transport


def create_test_harness() -> ReticulumTestHarness:
    """Create a new test harness."""
    return ReticulumTestHarness()
