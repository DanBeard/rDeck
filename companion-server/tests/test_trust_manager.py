"""Tests for TrustManager."""

import json
import pytest
from pathlib import Path
from unittest.mock import patch
import tempfile

from companion_server.trust_manager import TrustManager, TrustStatus, TrustedDevice


class TestTrustedDevice:
    """Test TrustedDevice dataclass."""

    def test_to_dict(self):
        device = TrustedDevice(
            hash="abcd1234",
            name="Test Device",
            status=TrustStatus.MUTUAL,
            offered_at=1000.0,
            accepted_at=2000.0,
            last_seen=3000.0,
        )

        d = device.to_dict()

        assert d["hash"] == "abcd1234"
        assert d["name"] == "Test Device"
        assert d["status"] == "mutual"
        assert d["offered_at"] == 1000.0
        assert d["accepted_at"] == 2000.0
        assert d["last_seen"] == 3000.0

    def test_from_dict(self):
        data = {
            "hash": "deadbeef",
            "name": "Another Device",
            "status": "offered",
            "offered_at": 500.0,
            "accepted_at": None,
            "last_seen": None,
        }

        device = TrustedDevice.from_dict(data)

        assert device.hash == "deadbeef"
        assert device.name == "Another Device"
        assert device.status == TrustStatus.OFFERED
        assert device.offered_at == 500.0
        assert device.accepted_at is None

    def test_roundtrip(self):
        original = TrustedDevice(
            hash="abc123",
            name="Device",
            status=TrustStatus.OFFERED,
            offered_at=100.0,
        )

        restored = TrustedDevice.from_dict(original.to_dict())

        assert restored.hash == original.hash
        assert restored.name == original.name
        assert restored.status == original.status


class TestTrustManager:
    """Test TrustManager functionality."""

    @pytest.fixture
    def temp_dir(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            yield Path(tmpdir)

    @pytest.fixture
    def manager(self, temp_dir):
        return TrustManager(temp_dir)

    def test_empty_initially(self, manager):
        assert manager.get_all_devices() == []
        assert manager.get_mutual_devices() == []
        assert manager.get_pending_devices() == []

    def test_offer_trust(self, manager):
        manager.offer_trust("hash123", "Test Device")

        devices = manager.get_all_devices()
        assert len(devices) == 1
        assert devices[0].hash == "hash123"
        assert devices[0].name == "Test Device"
        assert devices[0].status == TrustStatus.OFFERED

    def test_offer_trust_is_pending(self, manager):
        manager.offer_trust("hash456", "Device")

        assert manager.is_trust_pending("hash456")
        assert not manager.is_mutually_trusted("hash456")

    def test_accept_trust(self, manager):
        manager.offer_trust("hashxyz", "Device")
        manager.accept_trust("hashxyz", "Updated Name")

        assert manager.is_mutually_trusted("hashxyz")
        assert not manager.is_trust_pending("hashxyz")

        device = manager.get_device("hashxyz")
        assert device.name == "Updated Name"
        assert device.status == TrustStatus.MUTUAL
        assert device.accepted_at is not None

    def test_accept_trust_without_name_update(self, manager):
        manager.offer_trust("hash", "Original Name")
        manager.accept_trust("hash")

        device = manager.get_device("hash")
        assert device.name == "Original Name"

    def test_revoke_trust(self, manager):
        manager.offer_trust("hash", "Device")
        manager.accept_trust("hash")

        assert manager.is_mutually_trusted("hash")

        manager.revoke_trust("hash")

        assert not manager.is_mutually_trusted("hash")
        assert manager.get_device("hash") is None

    def test_revoke_nonexistent(self, manager):
        # Should not raise
        manager.revoke_trust("nonexistent")

    def test_update_last_seen(self, manager):
        manager.offer_trust("hash", "Device")

        assert manager.get_device("hash").last_seen is None

        manager.update_last_seen("hash")

        assert manager.get_device("hash").last_seen is not None

    def test_get_mutual_devices(self, manager):
        manager.offer_trust("hash1", "Device1")
        manager.offer_trust("hash2", "Device2")
        manager.offer_trust("hash3", "Device3")

        manager.accept_trust("hash1")
        manager.accept_trust("hash3")

        mutual = manager.get_mutual_devices()

        assert len(mutual) == 2
        hashes = [d.hash for d in mutual]
        assert "hash1" in hashes
        assert "hash3" in hashes
        assert "hash2" not in hashes

    def test_get_pending_devices(self, manager):
        manager.offer_trust("hash1", "Device1")
        manager.offer_trust("hash2", "Device2")
        manager.accept_trust("hash1")

        pending = manager.get_pending_devices()

        assert len(pending) == 1
        assert pending[0].hash == "hash2"

    def test_persistence(self, temp_dir):
        # Create manager and add data
        manager1 = TrustManager(temp_dir)
        manager1.offer_trust("persistent_hash", "Persistent Device")
        manager1.accept_trust("persistent_hash")

        # Create new manager from same directory
        manager2 = TrustManager(temp_dir)

        assert manager2.is_mutually_trusted("persistent_hash")
        device = manager2.get_device("persistent_hash")
        assert device.name == "Persistent Device"

    def test_persistence_file_format(self, temp_dir, manager):
        manager.offer_trust("hash1", "Device1")
        manager.accept_trust("hash1")

        trust_file = temp_dir / "trust.json"
        assert trust_file.exists()

        with open(trust_file) as f:
            data = json.load(f)

        assert "trusted_devices" in data
        assert "hash1" in data["trusted_devices"]
        assert data["trusted_devices"]["hash1"]["status"] == "mutual"

    def test_callback_on_change(self, manager):
        callback_count = [0]

        def callback():
            callback_count[0] += 1

        manager.on_change(callback)

        manager.offer_trust("hash", "Device")
        assert callback_count[0] == 1

        manager.accept_trust("hash")
        assert callback_count[0] == 2

        manager.revoke_trust("hash")
        assert callback_count[0] == 3

    def test_accept_unknown_device(self, manager):
        """Accept from device we didn't offer to (edge case)."""
        manager.accept_trust("unknown_hash", "Unknown Device")

        assert manager.is_mutually_trusted("unknown_hash")
        device = manager.get_device("unknown_hash")
        assert device.name == "Unknown Device"

    def test_offer_does_not_overwrite_trusted(self, manager):
        """Re-offering to already trusted device should not downgrade."""
        manager.offer_trust("hash", "Device")
        manager.accept_trust("hash")

        assert manager.is_mutually_trusted("hash")

        # Try to re-offer (e.g., server restarted)
        manager.offer_trust("hash", "New Name")

        # Should still be trusted
        assert manager.is_mutually_trusted("hash")
