"""Trust state management and persistence."""

import json
import time
import logging
from pathlib import Path
from dataclasses import dataclass, field, asdict
from enum import Enum
from typing import Optional
from threading import Lock

logger = logging.getLogger(__name__)


class TrustStatus(str, Enum):
    """Trust status states."""

    OFFERED = "offered"  # We sent trust offer, waiting for acceptance
    MUTUAL = "mutual"  # Both sides have accepted


@dataclass
class TrustedDevice:
    """Information about a trusted device."""

    hash: str
    name: str
    status: TrustStatus
    offered_at: float
    accepted_at: Optional[float] = None
    last_seen: Optional[float] = None

    def to_dict(self) -> dict:
        """Convert to JSON-serializable dict."""
        return {
            "hash": self.hash,
            "name": self.name,
            "status": self.status.value,
            "offered_at": self.offered_at,
            "accepted_at": self.accepted_at,
            "last_seen": self.last_seen,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "TrustedDevice":
        """Create from dict."""
        return cls(
            hash=data["hash"],
            name=data["name"],
            status=TrustStatus(data["status"]),
            offered_at=data.get("offered_at", 0),
            accepted_at=data.get("accepted_at"),
            last_seen=data.get("last_seen"),
        )


class TrustManager:
    """Manages trust relationships with devices."""

    def __init__(self, data_dir: Path):
        self.data_dir = Path(data_dir)
        self.trust_file = self.data_dir / "trust.json"
        self._lock = Lock()
        self._devices: dict[str, TrustedDevice] = {}
        self._callbacks: list[callable] = []

        self._load()

    def _load(self):
        """Load trust state from disk."""
        if not self.trust_file.exists():
            return

        try:
            with open(self.trust_file) as f:
                data = json.load(f)

            devices = data.get("trusted_devices", {})
            for hash_hex, device_data in devices.items():
                self._devices[hash_hex] = TrustedDevice.from_dict(device_data)

            logger.info(f"Loaded {len(self._devices)} trusted devices")

        except Exception as e:
            logger.exception(f"Error loading trust state: {e}")

    def _save(self):
        """Save trust state to disk."""
        try:
            data = {
                "trusted_devices": {
                    hash_hex: device.to_dict()
                    for hash_hex, device in self._devices.items()
                }
            }

            with open(self.trust_file, "w") as f:
                json.dump(data, f, indent=2)

        except Exception as e:
            logger.exception(f"Error saving trust state: {e}")

    def _notify_change(self):
        """Notify callbacks of trust state change."""
        for callback in self._callbacks:
            try:
                callback()
            except Exception:
                pass

    def offer_trust(self, hash_hex: str, name: str):
        """Record that we sent a trust offer to a device."""
        with self._lock:
            # Don't overwrite already trusted devices
            existing = self._devices.get(hash_hex)
            if existing and existing.status == TrustStatus.MUTUAL:
                return

            self._devices[hash_hex] = TrustedDevice(
                hash=hash_hex,
                name=name,
                status=TrustStatus.OFFERED,
                offered_at=time.time(),
            )
            self._save()
            self._notify_change()

    def accept_trust(self, hash_hex: str, name: Optional[str] = None):
        """Record that a device accepted our trust offer."""
        with self._lock:
            if hash_hex in self._devices:
                device = self._devices[hash_hex]
                device.status = TrustStatus.MUTUAL
                device.accepted_at = time.time()
                if name:
                    device.name = name
            else:
                # Device accepted without us sending offer first (shouldn't happen normally)
                self._devices[hash_hex] = TrustedDevice(
                    hash=hash_hex,
                    name=name or hash_hex[:12] + "...",
                    status=TrustStatus.MUTUAL,
                    offered_at=time.time(),
                    accepted_at=time.time(),
                )

            self._save()
            self._notify_change()

    def revoke_trust(self, hash_hex: str):
        """Remove trust for a device."""
        with self._lock:
            if hash_hex in self._devices:
                del self._devices[hash_hex]
                self._save()
                self._notify_change()

    def update_last_seen(self, hash_hex: str):
        """Update last seen timestamp for a device."""
        with self._lock:
            if hash_hex in self._devices:
                self._devices[hash_hex].last_seen = time.time()
                self._save()

    def is_mutually_trusted(self, hash_hex: str) -> bool:
        """Check if a device has mutual trust."""
        with self._lock:
            device = self._devices.get(hash_hex)
            return device is not None and device.status == TrustStatus.MUTUAL

    def is_trust_pending(self, hash_hex: str) -> bool:
        """Check if we have a pending trust offer to a device."""
        with self._lock:
            device = self._devices.get(hash_hex)
            return device is not None and device.status == TrustStatus.OFFERED

    def get_device(self, hash_hex: str) -> Optional[TrustedDevice]:
        """Get a trusted device by hash."""
        with self._lock:
            return self._devices.get(hash_hex)

    def get_all_devices(self) -> list[TrustedDevice]:
        """Get all trusted devices."""
        with self._lock:
            return list(self._devices.values())

    def get_mutual_devices(self) -> list[TrustedDevice]:
        """Get all devices with mutual trust."""
        with self._lock:
            return [d for d in self._devices.values() if d.status == TrustStatus.MUTUAL]

    def get_pending_devices(self) -> list[TrustedDevice]:
        """Get all devices with pending trust offers."""
        with self._lock:
            return [d for d in self._devices.values() if d.status == TrustStatus.OFFERED]

    def on_change(self, callback: callable):
        """Register a callback for trust state changes."""
        self._callbacks.append(callback)
