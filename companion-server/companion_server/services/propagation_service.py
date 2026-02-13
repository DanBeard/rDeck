"""Propagation service - LXMF store-and-forward via companion server."""

import json
import logging
import hashlib
from pathlib import Path
from typing import Any, Optional

from .base_service import BaseService
from ..protocol.messages import (
    PropSyncRequestPayload,
    PropSyncResponsePayload,
    PropMsgDeliverPayload,
    PropSubmitRequestPayload,
    PropSubmitResponsePayload,
)

logger = logging.getLogger(__name__)


class PropagationService(BaseService):
    """LXMF propagation node bridge.

    Enables rDeck to poll for stored messages and submit messages for
    propagation via the ServiceProtocol, while the companion server acts
    as a native LXMF propagation node.
    """

    def __init__(self, lxmf_router, data_dir: Path):
        self._router = lxmf_router
        self._data_dir = data_dir
        self._delivered_path = data_dir / "propagation_delivered.json"
        self._delivered: dict[str, list[str]] = {}  # device_hash_hex -> [transient_id_hex]
        self._load_delivered()

    @property
    def name(self) -> str:
        return "propagation"

    def handle_request(self, payload: Any) -> Any:
        """Not used directly - sync/submit have separate handlers."""
        raise NotImplementedError("Use handle_sync_request or handle_submit_request")

    def handle_sync_request(
        self, payload: PropSyncRequestPayload, device_hash_hex: str
    ) -> tuple[PropSyncResponsePayload, list[PropMsgDeliverPayload]]:
        """Scan propagation store for messages destined to the requesting device.

        Args:
            payload: Sync request with device's LXMF dest hash and known IDs
            device_hash_hex: Hex string of the requesting device's hash

        Returns:
            Tuple of (sync response, list of messages to deliver)
        """
        if not self._router:
            return PropSyncResponsePayload(count=0, error="Propagation not available"), []

        try:
            dest_hash = payload.lxmf_dest_hash
            known_ids_hex = {kid.hex() for kid in payload.known_ids}
            max_msgs = min(payload.max_messages, 20)  # Cap at 20

            # Also exclude previously delivered messages
            delivered_hex = set(self._delivered.get(device_hash_hex, []))

            messages = []

            # Scan propagation entries
            if hasattr(self._router, 'propagation_entries'):
                for transient_id, entry in self._router.propagation_entries.items():
                    if len(messages) >= max_msgs:
                        break

                    # entry[0] = destination_hash (bytes)
                    if entry[0] != dest_hash:
                        continue

                    tid_hex = transient_id.hex() if isinstance(transient_id, bytes) else str(transient_id)

                    # Skip known and already-delivered
                    if tid_hex in known_ids_hex or tid_hex in delivered_hex:
                        continue

                    # Read raw bytes from file
                    filepath = entry[1] if len(entry) > 1 else None
                    if filepath and Path(filepath).exists():
                        try:
                            raw_bytes = Path(filepath).read_bytes()
                            tid_bytes = transient_id if isinstance(transient_id, bytes) else bytes.fromhex(tid_hex)
                            messages.append(PropMsgDeliverPayload(
                                transient_id=tid_bytes,
                                raw_lxmf=raw_bytes,
                            ))
                        except Exception as e:
                            logger.warning(f"Failed to read propagation file {filepath}: {e}")

            response = PropSyncResponsePayload(count=len(messages))
            return response, messages

        except Exception as e:
            logger.exception(f"Error handling sync request: {e}")
            return PropSyncResponsePayload(count=0, error=str(e)), []

    def handle_submit_request(
        self, payload: PropSubmitRequestPayload
    ) -> PropSubmitResponsePayload:
        """Inject raw LXMF bytes into the propagation store.

        Args:
            payload: Submit request with raw LXMF bytes

        Returns:
            Submit response with acceptance status
        """
        if not self._router:
            return PropSubmitResponsePayload(accepted=False, error="Propagation not available")

        try:
            raw_bytes = payload.raw_lxmf
            if not raw_bytes or len(raw_bytes) < 96:  # Minimum: 16 dest + 16 src + 64 sig
                return PropSubmitResponsePayload(accepted=False, error="Invalid LXMF message")

            # Compute transient_id as SHA-256 of raw bytes
            transient_id = hashlib.sha256(raw_bytes).digest()

            # Inject into propagation store
            if hasattr(self._router, 'lxmf_propagation'):
                self._router.lxmf_propagation(
                    raw_bytes,
                    stamp_data=b'',
                    stamp_value=0,
                    is_paper_message=True,
                )
                logger.info(f"Injected message into propagation store: {transient_id.hex()[:16]}...")
                return PropSubmitResponsePayload(accepted=True, transient_id=transient_id)
            else:
                return PropSubmitResponsePayload(accepted=False, error="Propagation not supported by router")

        except Exception as e:
            logger.exception(f"Error handling submit request: {e}")
            return PropSubmitResponsePayload(accepted=False, error=str(e))

    def mark_delivered(self, device_hash_hex: str, transient_ids: list[bytes]):
        """Mark messages as delivered to avoid re-sending on next sync."""
        if device_hash_hex not in self._delivered:
            self._delivered[device_hash_hex] = []

        for tid in transient_ids:
            tid_hex = tid.hex()
            if tid_hex not in self._delivered[device_hash_hex]:
                self._delivered[device_hash_hex].append(tid_hex)

        # Cap stored IDs per device
        max_per_device = 200
        if len(self._delivered[device_hash_hex]) > max_per_device:
            self._delivered[device_hash_hex] = self._delivered[device_hash_hex][-max_per_device:]

        self._save_delivered()

    def _load_delivered(self):
        """Load delivered message tracking from disk."""
        try:
            if self._delivered_path.exists():
                with open(self._delivered_path) as f:
                    self._delivered = json.load(f)
        except Exception as e:
            logger.warning(f"Failed to load delivered tracking: {e}")
            self._delivered = {}

    def _save_delivered(self):
        """Save delivered message tracking to disk."""
        try:
            with open(self._delivered_path, "w") as f:
                json.dump(self._delivered, f)
        except Exception as e:
            logger.warning(f"Failed to save delivered tracking: {e}")
