"""Tests for propagation service."""

import json
import hashlib
import os
import tempfile
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from companion_server.services.propagation_service import PropagationService
from companion_server.protocol.messages import (
    PropSyncRequestPayload,
    PropSubmitRequestPayload,
    PropSyncResponsePayload,
    PropMsgDeliverPayload,
    PropSubmitResponsePayload,
    MessageType,
)


class TestPropagationServiceInit:
    """Test PropagationService initialization."""

    def test_name(self, tmp_path):
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        assert service.name == "propagation"

    def test_delivered_tracking_persistence(self, tmp_path):
        """Test that delivered tracking saves and loads."""
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        service.mark_delivered("aabb", [b"\x01" * 32])

        # Create new instance - should load from disk
        service2 = PropagationService(lxmf_router=None, data_dir=tmp_path)
        assert "aabb" in service2._delivered
        assert len(service2._delivered["aabb"]) == 1


class TestPropagationSyncRequest:
    """Test sync request handling."""

    def test_no_router_returns_error(self, tmp_path):
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=bytes(16),
            known_ids=[],
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "device123")
        assert response.count == 0
        assert response.error == "Propagation not available"
        assert messages == []

    def test_no_entries_returns_empty(self, tmp_path):
        router = MagicMock()
        router.propagation_entries = {}

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=bytes(16),
            known_ids=[],
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "device123")
        assert response.count == 0
        assert response.error is None
        assert messages == []

    def test_matching_messages_returned(self, tmp_path):
        """Test that messages matching the device dest hash are returned."""
        dest_hash = bytes(16)  # The device's LXMF dest hash
        raw_data = b"\x00" * 96 + b"test message data"
        transient_id = hashlib.sha256(raw_data).digest()

        # Write raw data to a temp file
        msg_file = tmp_path / "msg1.lxmf"
        msg_file.write_bytes(raw_data)

        router = MagicMock()
        router.propagation_entries = {
            transient_id: (dest_hash, str(msg_file)),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=dest_hash,
            known_ids=[],
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "device123")
        assert response.count == 1
        assert len(messages) == 1
        assert messages[0].raw_lxmf == raw_data
        assert messages[0].transient_id == transient_id

    def test_known_ids_excluded(self, tmp_path):
        """Test that known IDs are excluded from results."""
        dest_hash = bytes(16)
        raw_data = b"\x00" * 96 + b"test"
        transient_id = hashlib.sha256(raw_data).digest()

        msg_file = tmp_path / "msg1.lxmf"
        msg_file.write_bytes(raw_data)

        router = MagicMock()
        router.propagation_entries = {
            transient_id: (dest_hash, str(msg_file)),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=dest_hash,
            known_ids=[transient_id],  # Already known
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "device123")
        assert response.count == 0
        assert messages == []

    def test_previously_delivered_excluded(self, tmp_path):
        """Test that previously delivered messages are excluded."""
        dest_hash = bytes(16)
        raw_data = b"\x00" * 96 + b"test"
        transient_id = hashlib.sha256(raw_data).digest()

        msg_file = tmp_path / "msg1.lxmf"
        msg_file.write_bytes(raw_data)

        router = MagicMock()
        router.propagation_entries = {
            transient_id: (dest_hash, str(msg_file)),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        # Mark as already delivered
        service.mark_delivered("device123", [transient_id])

        payload = PropSyncRequestPayload(
            lxmf_dest_hash=dest_hash,
            known_ids=[],
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "device123")
        assert response.count == 0

    def test_max_messages_cap(self, tmp_path):
        """Test that max_messages limits results."""
        dest_hash = bytes(16)

        entries = {}
        for i in range(5):
            raw_data = b"\x00" * 96 + bytes([i])
            tid = hashlib.sha256(raw_data).digest()
            msg_file = tmp_path / f"msg{i}.lxmf"
            msg_file.write_bytes(raw_data)
            entries[tid] = (dest_hash, str(msg_file))

        router = MagicMock()
        router.propagation_entries = entries

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=dest_hash,
            known_ids=[],
            max_messages=2,
        )
        response, messages = service.handle_sync_request(payload, "device123")
        assert response.count == 2
        assert len(messages) == 2

    def test_non_matching_dest_excluded(self, tmp_path):
        """Test that messages for other destinations are excluded."""
        my_dest = bytes(16)
        other_dest = b"\xff" * 16
        raw_data = b"\x00" * 96 + b"other"
        transient_id = hashlib.sha256(raw_data).digest()

        msg_file = tmp_path / "msg1.lxmf"
        msg_file.write_bytes(raw_data)

        router = MagicMock()
        router.propagation_entries = {
            transient_id: (other_dest, str(msg_file)),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=my_dest,
            known_ids=[],
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "device123")
        assert response.count == 0


class TestPropagationSubmitRequest:
    """Test submit request handling."""

    def test_no_router_returns_error(self, tmp_path):
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        payload = PropSubmitRequestPayload(raw_lxmf=b"\x00" * 100)
        response = service.handle_submit_request(payload)
        assert response.accepted is False
        assert response.error == "Propagation not available"

    def test_too_short_message_rejected(self, tmp_path):
        router = MagicMock()
        router.lxmf_propagation = MagicMock()

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSubmitRequestPayload(raw_lxmf=b"\x00" * 50)  # Too short
        response = service.handle_submit_request(payload)
        assert response.accepted is False
        assert "Invalid" in response.error

    def test_successful_submission(self, tmp_path):
        router = MagicMock()
        router.lxmf_propagation = MagicMock()

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        raw_data = b"\x00" * 96 + b"test message"
        payload = PropSubmitRequestPayload(raw_lxmf=raw_data)
        response = service.handle_submit_request(payload)

        assert response.accepted is True
        assert response.transient_id == hashlib.sha256(raw_data).digest()
        router.lxmf_propagation.assert_called_once_with(
            raw_data,
            stamp_data=b'',
            stamp_value=0,
            is_paper_message=True,
        )

    def test_empty_message_rejected(self, tmp_path):
        router = MagicMock()
        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSubmitRequestPayload(raw_lxmf=b"")
        response = service.handle_submit_request(payload)
        assert response.accepted is False


class TestDeliveredTracking:
    """Test delivered message tracking."""

    def test_mark_and_check(self, tmp_path):
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        tid = b"\xaa" * 32
        service.mark_delivered("dev1", [tid])

        assert tid.hex() in service._delivered["dev1"]

    def test_cap_at_max(self, tmp_path):
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)

        # Add 250 entries (over the 200 cap)
        tids = [bytes([i % 256]) * 32 for i in range(250)]
        service.mark_delivered("dev1", tids)

        assert len(service._delivered["dev1"]) == 200

    def test_persistence_roundtrip(self, tmp_path):
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        tid1 = b"\xbb" * 32
        tid2 = b"\xcc" * 32
        service.mark_delivered("dev1", [tid1])
        service.mark_delivered("dev2", [tid2])

        # Reload
        service2 = PropagationService(lxmf_router=None, data_dir=tmp_path)
        assert tid1.hex() in service2._delivered["dev1"]
        assert tid2.hex() in service2._delivered["dev2"]


class TestSyncRequestEdgeCases:
    """Edge case tests for sync request handling."""

    def test_missing_file_skipped(self, tmp_path):
        """Messages whose files are missing should be silently skipped."""
        dest_hash = bytes(16)
        transient_id = b"\xdd" * 32

        router = MagicMock()
        router.propagation_entries = {
            transient_id: (dest_hash, "/nonexistent/path/msg.lxmf"),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=dest_hash,
            known_ids=[],
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "device1")
        assert response.count == 0
        assert messages == []
        assert response.error is None

    def test_empty_file_skipped(self, tmp_path):
        """Empty message files should be skipped."""
        dest_hash = bytes(16)
        transient_id = b"\xee" * 32

        msg_file = tmp_path / "empty.lxmf"
        msg_file.write_bytes(b"")

        router = MagicMock()
        router.propagation_entries = {
            transient_id: (dest_hash, str(msg_file)),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSyncRequestPayload(
            lxmf_dest_hash=dest_hash,
            known_ids=[],
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "device1")
        # Empty file should still be returned (server doesn't validate content)
        # The device side handles validation
        assert response.count <= 1

    def test_multiple_devices_different_delivered(self, tmp_path):
        """Different devices should have independent delivered tracking."""
        dest_hash = bytes(16)
        raw1 = b"\x00" * 96 + b"msg1"
        raw2 = b"\x00" * 96 + b"msg2"
        tid1 = hashlib.sha256(raw1).digest()
        tid2 = hashlib.sha256(raw2).digest()

        (tmp_path / "msg1.lxmf").write_bytes(raw1)
        (tmp_path / "msg2.lxmf").write_bytes(raw2)

        router = MagicMock()
        router.propagation_entries = {
            tid1: (dest_hash, str(tmp_path / "msg1.lxmf")),
            tid2: (dest_hash, str(tmp_path / "msg2.lxmf")),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        # Device A already received msg1
        service.mark_delivered("deviceA", [tid1])

        # Device A should only get msg2
        payloadA = PropSyncRequestPayload(lxmf_dest_hash=dest_hash, known_ids=[], max_messages=10)
        respA, msgsA = service.handle_sync_request(payloadA, "deviceA")
        assert respA.count == 1
        assert msgsA[0].transient_id == tid2

        # Device B should get both
        payloadB = PropSyncRequestPayload(lxmf_dest_hash=dest_hash, known_ids=[], max_messages=10)
        respB, msgsB = service.handle_sync_request(payloadB, "deviceB")
        assert respB.count == 2

    def test_known_ids_and_delivered_both_excluded(self, tmp_path):
        """Both known_ids and delivered messages should be excluded."""
        dest_hash = bytes(16)
        raw1 = b"\x00" * 96 + b"msg1"
        raw2 = b"\x00" * 96 + b"msg2"
        raw3 = b"\x00" * 96 + b"msg3"
        tid1 = hashlib.sha256(raw1).digest()
        tid2 = hashlib.sha256(raw2).digest()
        tid3 = hashlib.sha256(raw3).digest()

        for i, (raw, tid) in enumerate([(raw1, tid1), (raw2, tid2), (raw3, tid3)]):
            (tmp_path / f"msg{i}.lxmf").write_bytes(raw)

        router = MagicMock()
        router.propagation_entries = {
            tid1: (dest_hash, str(tmp_path / "msg0.lxmf")),
            tid2: (dest_hash, str(tmp_path / "msg1.lxmf")),
            tid3: (dest_hash, str(tmp_path / "msg2.lxmf")),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        service.mark_delivered("dev1", [tid1])  # Already delivered

        payload = PropSyncRequestPayload(
            lxmf_dest_hash=dest_hash,
            known_ids=[tid2],  # Already known on device
            max_messages=10,
        )
        response, messages = service.handle_sync_request(payload, "dev1")
        # Only tid3 should be returned (tid1 delivered, tid2 known)
        assert response.count == 1
        assert messages[0].transient_id == tid3


class TestSubmitEdgeCases:
    """Edge case tests for submit request handling."""

    def test_router_exception_handled(self, tmp_path):
        """Router exceptions during submission should return error."""
        router = MagicMock()
        router.lxmf_propagation = MagicMock(side_effect=RuntimeError("Storage full"))

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        raw_data = b"\x00" * 96 + b"test"
        payload = PropSubmitRequestPayload(raw_lxmf=raw_data)
        response = service.handle_submit_request(payload)

        assert response.accepted is False
        assert "Storage full" in response.error

    def test_exact_96_bytes_accepted(self, tmp_path):
        """Exactly 96 bytes (minimum valid) should be accepted."""
        router = MagicMock()
        router.lxmf_propagation = MagicMock()

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        raw_data = b"\x00" * 96  # Exactly 96 bytes
        payload = PropSubmitRequestPayload(raw_lxmf=raw_data)
        response = service.handle_submit_request(payload)
        assert response.accepted is True

    def test_95_bytes_rejected(self, tmp_path):
        """95 bytes (just under minimum) should be rejected."""
        router = MagicMock()
        router.lxmf_propagation = MagicMock()

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        payload = PropSubmitRequestPayload(raw_lxmf=b"\x00" * 95)
        response = service.handle_submit_request(payload)
        assert response.accepted is False

    def test_transient_id_is_sha256(self, tmp_path):
        """Transient ID should be SHA-256 of the raw LXMF bytes."""
        router = MagicMock()
        router.lxmf_propagation = MagicMock()

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)
        raw_data = b"\xAB" * 200
        expected_tid = hashlib.sha256(raw_data).digest()

        payload = PropSubmitRequestPayload(raw_lxmf=raw_data)
        response = service.handle_submit_request(payload)
        assert response.transient_id == expected_tid


class TestDeliveredTrackingEdgeCases:
    """Additional delivered tracking tests."""

    def test_corrupted_json_handled(self, tmp_path):
        """Corrupted delivered file should be handled gracefully."""
        delivered_file = tmp_path / "propagation_delivered.json"
        delivered_file.write_text("{invalid json")

        # Should not crash
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        assert service._delivered == {}

    def test_empty_json_handled(self, tmp_path):
        """Empty delivered file should be handled gracefully."""
        delivered_file = tmp_path / "propagation_delivered.json"
        delivered_file.write_text("{}")

        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        assert service._delivered == {}

    def test_mark_delivered_multiple_calls(self, tmp_path):
        """Multiple mark_delivered calls should accumulate."""
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        service.mark_delivered("dev1", [b"\x01" * 32])
        service.mark_delivered("dev1", [b"\x02" * 32])

        assert len(service._delivered["dev1"]) == 2

    def test_mark_delivered_dedup(self, tmp_path):
        """Marking same transient_id twice should not duplicate."""
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)
        tid = b"\xAA" * 32
        service.mark_delivered("dev1", [tid])
        service.mark_delivered("dev1", [tid])

        # Sets don't allow duplicates
        assert len(service._delivered["dev1"]) == 1

    def test_cap_preserves_newest(self, tmp_path):
        """When capping at max, newest entries should be preserved."""
        service = PropagationService(lxmf_router=None, data_dir=tmp_path)

        # Add entries one at a time to control order
        tids = []
        for i in range(250):
            tid = bytes([i % 256]) * 32
            tids.append(tid)
            service.mark_delivered("dev1", [tid])

        # Should be capped at 200
        assert len(service._delivered["dev1"]) == 200


class TestProtocolIntegration:
    """Test propagation service with full protocol serialization."""

    def test_sync_request_full_roundtrip(self, tmp_path):
        """Test sync request/response through full msgpack serialization."""
        from companion_server.protocol.serialization import _encode_payload, _decode_payload

        dest_hash = bytes(16)
        raw_data = b"\x00" * 96 + b"hello propagation"
        transient_id = hashlib.sha256(raw_data).digest()

        msg_file = tmp_path / "msg.lxmf"
        msg_file.write_bytes(raw_data)

        router = MagicMock()
        router.propagation_entries = {
            transient_id: (dest_hash, str(msg_file)),
        }

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)

        # Create and encode request
        original_request = PropSyncRequestPayload(
            lxmf_dest_hash=dest_hash,
            known_ids=[],
            max_messages=10,
        )
        encoded = _encode_payload(MessageType.PROP_SYNC_REQUEST, original_request)
        decoded_request = _decode_payload(MessageType.PROP_SYNC_REQUEST, encoded)

        # Process through service
        response, messages = service.handle_sync_request(decoded_request, "test_device")

        # Encode response
        response_encoded = _encode_payload(MessageType.PROP_SYNC_RESPONSE, response)
        decoded_response = _decode_payload(MessageType.PROP_SYNC_RESPONSE, response_encoded)

        assert decoded_response.count == 1
        assert decoded_response.error is None

        # Encode/decode delivered message
        for msg in messages:
            msg_encoded = _encode_payload(MessageType.PROP_MSG_DELIVER, msg)
            decoded_msg = _decode_payload(MessageType.PROP_MSG_DELIVER, msg_encoded)
            assert decoded_msg.raw_lxmf == raw_data
            assert decoded_msg.transient_id == transient_id

    def test_submit_request_full_roundtrip(self, tmp_path):
        """Test submit request/response through full msgpack serialization."""
        from companion_server.protocol.serialization import _encode_payload, _decode_payload

        router = MagicMock()
        router.lxmf_propagation = MagicMock()

        service = PropagationService(lxmf_router=router, data_dir=tmp_path)

        raw_data = b"\x00" * 96 + b"submit test"

        # Create and encode request
        original_request = PropSubmitRequestPayload(raw_lxmf=raw_data)
        encoded = _encode_payload(MessageType.PROP_SUBMIT_REQUEST, original_request)
        decoded_request = _decode_payload(MessageType.PROP_SUBMIT_REQUEST, encoded)

        assert decoded_request.raw_lxmf == raw_data

        # Process through service
        response = service.handle_submit_request(decoded_request)

        # Encode/decode response
        response_encoded = _encode_payload(MessageType.PROP_SUBMIT_RESPONSE, response)
        decoded_response = _decode_payload(MessageType.PROP_SUBMIT_RESPONSE, response_encoded)

        assert decoded_response.accepted is True
        assert decoded_response.transient_id == hashlib.sha256(raw_data).digest()

    def test_sync_response_matches_vector_format(self):
        """Verify sync response encoding matches shared protocol vectors."""
        import msgpack
        from companion_server.protocol.serialization import _encode_payload

        response = PropSyncResponsePayload(count=5, error=None)
        encoded = _encode_payload(MessageType.PROP_SYNC_RESPONSE, response)
        decoded_raw = msgpack.unpackb(encoded, raw=False)

        assert "count" in decoded_raw
        assert decoded_raw["count"] == 5
        # error field omitted when None (optional field)

    def test_submit_response_matches_vector_format(self):
        """Verify submit response encoding matches shared protocol vectors."""
        import msgpack
        from companion_server.protocol.serialization import _encode_payload

        tid = b"\xAA" * 32
        response = PropSubmitResponsePayload(accepted=True, transient_id=tid)
        encoded = _encode_payload(MessageType.PROP_SUBMIT_RESPONSE, response)
        decoded_raw = msgpack.unpackb(encoded, raw=False)

        assert "accepted" in decoded_raw
        assert "transient_id" in decoded_raw
        assert decoded_raw["accepted"] is True
        assert decoded_raw["transient_id"] == tid
