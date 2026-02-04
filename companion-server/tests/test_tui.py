"""Tests for TUI components using Textual's test framework."""

import pytest
from unittest.mock import MagicMock, AsyncMock

from textual.app import App, ComposeResult
from textual.widgets import Static, Button, Label

from companion_server.trust_manager import TrustedDevice, TrustStatus, TrustManager
from companion_server.reticulum_service import AnnounceInfo
from companion_server.tui.app import CompanionServerApp, LogPanel
from companion_server.tui.announce_view import AnnounceView, AnnounceItem
from companion_server.tui.trust_view import TrustView, TrustItem


# ============================================================
# Test App Wrappers (run_test is only available on App)
# ============================================================


class LogPanelTestApp(App):
    """Test wrapper for LogPanel."""

    def compose(self) -> ComposeResult:
        yield LogPanel(id="log")


class AnnounceItemTestApp(App):
    """Test wrapper for AnnounceItem."""

    def __init__(self, announce: AnnounceInfo):
        super().__init__()
        self.announce = announce

    def compose(self) -> ComposeResult:
        yield AnnounceItem(self.announce)


class AnnounceViewTestApp(App):
    """Test wrapper for AnnounceView."""

    def compose(self) -> ComposeResult:
        yield AnnounceView(id="announces")


class TrustItemTestApp(App):
    """Test wrapper for TrustItem."""

    def __init__(self, device: TrustedDevice):
        super().__init__()
        self.device = device

    def compose(self) -> ComposeResult:
        yield TrustItem(self.device)


class TrustViewTestApp(App):
    """Test wrapper for TrustView."""

    def compose(self) -> ComposeResult:
        yield TrustView(id="trusted")


# ============================================================
# LogPanel Tests
# ============================================================


class TestLogPanel:
    """Tests for the LogPanel widget."""

    @pytest.mark.asyncio
    async def test_log_panel_initial_empty(self):
        """LogPanel should be empty initially."""
        async with LogPanelTestApp().run_test() as pilot:
            panel = pilot.app.query_one(LogPanel)
            assert panel._messages == []

    @pytest.mark.asyncio
    async def test_log_panel_add_log(self):
        """add_log should add a timestamped message."""
        async with LogPanelTestApp().run_test() as pilot:
            panel = pilot.app.query_one(LogPanel)
            panel.add_log("Test message")
            assert len(panel._messages) == 1
            assert "Test message" in panel._messages[0]
            # Should have timestamp
            assert "[" in panel._messages[0] and "]" in panel._messages[0]

    @pytest.mark.asyncio
    async def test_log_panel_max_messages(self):
        """LogPanel should limit messages to max_messages."""
        async with LogPanelTestApp().run_test() as pilot:
            panel = pilot.app.query_one(LogPanel)
            for i in range(150):
                panel.add_log(f"Message {i}")
            assert len(panel._messages) == 100  # Default max


# ============================================================
# AnnounceItem Tests
# ============================================================


class TestAnnounceItem:
    """Tests for the AnnounceItem widget."""

    @pytest.fixture
    def sample_announce(self):
        return AnnounceInfo(
            destination_hash=b"\x01" * 16,
            display_name="TestDevice",
            app_data=b"",
            timestamp=1234567890.0,
        )

    @pytest.mark.asyncio
    async def test_announce_item_displays_name(self, sample_announce):
        """AnnounceItem should display the device name."""
        async with AnnounceItemTestApp(sample_announce).run_test() as pilot:
            # Query for the name label
            labels = pilot.app.query(Label)
            names = [l for l in labels if "name" in l.classes]
            assert len(names) == 1
            assert names[0].render() == "TestDevice"

    @pytest.mark.asyncio
    async def test_announce_item_displays_hash(self, sample_announce):
        """AnnounceItem should display truncated hash."""
        async with AnnounceItemTestApp(sample_announce).run_test() as pilot:
            labels = pilot.app.query(Label)
            hashes = [l for l in labels if "hash" in l.classes]
            assert len(hashes) == 1
            assert "..." in str(hashes[0].render())

    @pytest.mark.asyncio
    async def test_announce_item_has_trust_button(self, sample_announce):
        """AnnounceItem should have a Trust button."""
        async with AnnounceItemTestApp(sample_announce).run_test() as pilot:
            buttons = pilot.app.query(Button)
            assert len(list(buttons)) == 1
            button = list(buttons)[0]
            assert "Trust" in str(button.label)

    @pytest.mark.asyncio
    async def test_announce_item_trust_click_message(self, sample_announce):
        """Clicking Trust should post TrustClicked message."""
        messages = []

        async with AnnounceItemTestApp(sample_announce).run_test() as pilot:
            item = pilot.app.query_one(AnnounceItem)

            # Capture messages
            original_post = item.post_message

            def capture_post(msg):
                messages.append(msg)
                return original_post(msg)

            item.post_message = capture_post

            # Click the trust button
            button = pilot.app.query_one(Button)
            await pilot.click(button)

        trust_messages = [m for m in messages if isinstance(m, AnnounceItem.TrustClicked)]
        assert len(trust_messages) == 1
        assert trust_messages[0].announce == sample_announce


# ============================================================
# AnnounceView Tests
# ============================================================


class TestAnnounceView:
    """Tests for the AnnounceView widget."""

    @pytest.fixture
    def sample_announces(self):
        return [
            AnnounceInfo(
                destination_hash=bytes([i] * 16),
                display_name=f"Device{i}",
                app_data=b"",
                timestamp=1234567890.0 + i,
            )
            for i in range(3)
        ]

    @pytest.mark.asyncio
    async def test_announce_view_initial_empty(self):
        """AnnounceView should show empty message initially."""
        async with AnnounceViewTestApp().run_test() as pilot:
            statics = pilot.app.query(Static)
            empty_msgs = [s for s in statics if "empty" in s.classes]
            assert len(empty_msgs) == 1
            assert "Waiting" in str(empty_msgs[0].render())

    @pytest.mark.asyncio
    async def test_announce_view_add_announce(self, sample_announces):
        """add_announce should add an announce item."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            view.add_announce(sample_announces[0])
            await pilot.pause()

            items = pilot.app.query(AnnounceItem)
            assert len(list(items)) == 1

    @pytest.mark.asyncio
    async def test_announce_view_multiple_announces(self, sample_announces):
        """AnnounceView should display multiple announces."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            for announce in sample_announces:
                view.add_announce(announce)
            await pilot.pause()

            items = pilot.app.query(AnnounceItem)
            assert len(list(items)) == 3

    @pytest.mark.asyncio
    async def test_announce_view_updates_existing(self, sample_announces):
        """Adding same hash should update, not duplicate."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)

            # Add first announce
            view.add_announce(sample_announces[0])
            await pilot.pause()

            # Update same device with new name
            updated = AnnounceInfo(
                destination_hash=sample_announces[0].destination_hash,
                display_name="UpdatedName",
                app_data=b"",
                timestamp=1234567899.0,
            )
            view.add_announce(updated)
            await pilot.pause()

            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 1

    @pytest.mark.asyncio
    async def test_announce_view_known_only_filter(self):
        """Known only checkbox should filter to known device types."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)

            # Add known device (companion server)
            known_device = AnnounceInfo(
                destination_hash=b"\x01" * 16,
                display_name="CompanionServer",
                app_data=b"",
                timestamp=1234567890.0,
                device_type="companion-server",
                services=["ntp", "search"],
            )
            view.add_announce(known_device)

            # Add unknown device
            unknown_device = AnnounceInfo(
                destination_hash=b"\x02" * 16,
                display_name="UnknownDevice",
                app_data=b"",
                timestamp=1234567891.0,
                device_type=None,
            )
            view.add_announce(unknown_device)
            await pilot.pause()

            # Both should be visible initially
            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 2

            # Enable known only filter
            view._known_only = True
            view._refresh_list()
            await pilot.pause()

            # Only known device should be visible
            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 1
            assert items[0].announce.display_name == "CompanionServer"

    @pytest.mark.asyncio
    async def test_announce_view_filter_by_device_type(self):
        """Text filter should match device type."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)

            # Add companion server
            server = AnnounceInfo(
                destination_hash=b"\x01" * 16,
                display_name="MyServer",
                app_data=b"",
                timestamp=1234567890.0,
                device_type="companion-server",
            )
            view.add_announce(server)

            # Add rdeck device
            rdeck = AnnounceInfo(
                destination_hash=b"\x02" * 16,
                display_name="MyRdeck",
                app_data=b"",
                timestamp=1234567891.0,
                device_type="rdeck",
            )
            view.add_announce(rdeck)
            await pilot.pause()

            # Both visible initially
            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 2

            # Filter by device type
            view._filter_text = "companion"
            view._refresh_list()
            await pilot.pause()

            # Only server should be visible
            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 1
            assert items[0].announce.display_name == "MyServer"


class TestAnnounceViewFilterInteraction:
    """Tests for filter input and checkbox interactions."""

    @pytest.fixture
    def sample_announces(self):
        return [
            AnnounceInfo(
                destination_hash=bytes([i] * 16),
                display_name=f"Device{i}",
                app_data=b"",
                timestamp=1234567890.0 + i,
                device_type="rdeck" if i % 2 == 0 else None,
            )
            for i in range(5)
        ]

    @pytest.mark.asyncio
    async def test_filter_input_typing(self, sample_announces):
        """Typing in filter input should filter the list."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            for announce in sample_announces:
                view.add_announce(announce)
            await pilot.pause()

            # All 5 should be visible
            assert len(list(pilot.app.query(AnnounceItem))) == 5

            # Type in filter - simulate input change
            from textual.widgets import Input
            filter_input = view.query_one("#filter-input", Input)
            filter_input.value = "Device2"
            await pilot.pause()

            # Only Device2 should match
            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 1
            assert items[0].announce.display_name == "Device2"

    @pytest.mark.asyncio
    async def test_filter_input_clear(self, sample_announces):
        """Clearing filter should show all items."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            for announce in sample_announces:
                view.add_announce(announce)
            await pilot.pause()

            # Apply filter
            from textual.widgets import Input
            filter_input = view.query_one("#filter-input", Input)
            filter_input.value = "Device1"
            await pilot.pause()
            assert len(list(pilot.app.query(AnnounceItem))) == 1

            # Clear filter
            filter_input.value = ""
            await pilot.pause()
            assert len(list(pilot.app.query(AnnounceItem))) == 5

    @pytest.mark.asyncio
    async def test_filter_input_no_match(self, sample_announces):
        """Filter with no matches should show empty message."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            for announce in sample_announces:
                view.add_announce(announce)
            await pilot.pause()

            from textual.widgets import Input
            filter_input = view.query_one("#filter-input", Input)
            filter_input.value = "nonexistent"
            await pilot.pause()

            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 0

            # Should show "No matches" message
            statics = pilot.app.query(Static)
            empty_msgs = [s for s in statics if "empty" in s.classes]
            assert len(empty_msgs) == 1
            assert "No matches" in str(empty_msgs[0].render())

    @pytest.mark.asyncio
    async def test_checkbox_toggle(self, sample_announces):
        """Toggling checkbox should filter known devices."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            for announce in sample_announces:
                view.add_announce(announce)
            await pilot.pause()

            # All 5 visible initially
            assert len(list(pilot.app.query(AnnounceItem))) == 5

            # Toggle checkbox on
            from textual.widgets import Checkbox
            checkbox = view.query_one("#known-only-checkbox", Checkbox)
            checkbox.value = True
            await pilot.pause()

            # Only known types (device_type != None) - Device0, Device2, Device4
            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 3

            # Toggle checkbox off
            checkbox.value = False
            await pilot.pause()
            assert len(list(pilot.app.query(AnnounceItem))) == 5

    @pytest.mark.asyncio
    async def test_combined_filter_and_checkbox(self, sample_announces):
        """Text filter and checkbox should work together."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            for announce in sample_announces:
                view.add_announce(announce)
            await pilot.pause()

            from textual.widgets import Input, Checkbox
            filter_input = view.query_one("#filter-input", Input)
            checkbox = view.query_one("#known-only-checkbox", Checkbox)

            # Filter by "Device" and known only
            filter_input.value = "Device"
            checkbox.value = True
            await pilot.pause()

            # Should match Device0, Device2, Device4 (known types matching "Device")
            items = list(pilot.app.query(AnnounceItem))
            assert len(items) == 3

    @pytest.mark.asyncio
    async def test_rapid_filter_changes(self, sample_announces):
        """Rapid filter changes should not cause issues."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            for announce in sample_announces:
                view.add_announce(announce)
            await pilot.pause()

            from textual.widgets import Input
            filter_input = view.query_one("#filter-input", Input)

            # Rapid changes
            for i in range(10):
                filter_input.value = f"test{i}"
            await pilot.pause()

            # Clear
            filter_input.value = ""
            await pilot.pause()

            # Should recover and show all
            assert len(list(pilot.app.query(AnnounceItem))) == 5

    @pytest.mark.asyncio
    async def test_focus_filter_method(self):
        """focus_filter should focus the input."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            view.focus_filter()
            await pilot.pause()

            from textual.widgets import Input
            filter_input = view.query_one("#filter-input", Input)
            assert filter_input.has_focus

    @pytest.mark.asyncio
    async def test_clear_filter_method(self, sample_announces):
        """clear_filter should reset filter state."""
        async with AnnounceViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(AnnounceView)
            for announce in sample_announces:
                view.add_announce(announce)
            await pilot.pause()

            from textual.widgets import Input
            filter_input = view.query_one("#filter-input", Input)
            filter_input.value = "Device1"
            await pilot.pause()
            assert len(list(pilot.app.query(AnnounceItem))) == 1

            # Use clear_filter method
            view.clear_filter()
            await pilot.pause()

            assert filter_input.value == ""
            assert len(list(pilot.app.query(AnnounceItem))) == 5


class TestAnnounceItemDeviceType:
    """Tests for device type display in AnnounceItem."""

    @pytest.mark.asyncio
    async def test_announce_item_shows_device_type_badge(self):
        """AnnounceItem should show device type badge for known types."""
        announce = AnnounceInfo(
            destination_hash=b"\x01" * 16,
            display_name="TestServer",
            app_data=b"",
            timestamp=1234567890.0,
            device_type="companion-server",
            services=["ntp", "search"],
        )
        async with AnnounceItemTestApp(announce).run_test() as pilot:
            labels = pilot.app.query(Label)
            device_type_labels = [l for l in labels if "device-type" in l.classes]
            assert len(device_type_labels) == 1
            rendered = str(device_type_labels[0].render())
            assert "companion-server" in rendered
            assert "ntp" in rendered
            assert "search" in rendered

    @pytest.mark.asyncio
    async def test_announce_item_no_badge_for_unknown(self):
        """AnnounceItem should not show device type badge for unknown types."""
        announce = AnnounceInfo(
            destination_hash=b"\x01" * 16,
            display_name="UnknownDevice",
            app_data=b"",
            timestamp=1234567890.0,
            device_type=None,
        )
        async with AnnounceItemTestApp(announce).run_test() as pilot:
            labels = pilot.app.query(Label)
            device_type_labels = [l for l in labels if "device-type" in l.classes]
            assert len(device_type_labels) == 0


# ============================================================
# TrustItem Tests
# ============================================================


class TestTrustItem:
    """Tests for the TrustItem widget."""

    @pytest.fixture
    def mutual_device(self):
        return TrustedDevice(
            hash="0102030405060708090a0b0c0d0e0f10",
            name="TrustedDevice",
            status=TrustStatus.MUTUAL,
            offered_at=1234567890.0,
            accepted_at=1234567900.0,
        )

    @pytest.fixture
    def pending_device(self):
        return TrustedDevice(
            hash="1112131415161718191a1b1c1d1e1f20",
            name="PendingDevice",
            status=TrustStatus.OFFERED,
            offered_at=1234567890.0,
        )

    @pytest.mark.asyncio
    async def test_trust_item_displays_name(self, mutual_device):
        """TrustItem should display the device name."""
        async with TrustItemTestApp(mutual_device).run_test() as pilot:
            labels = pilot.app.query(Label)
            names = [l for l in labels if "name" in l.classes]
            assert len(names) == 1
            assert names[0].render() == "TrustedDevice"

    @pytest.mark.asyncio
    async def test_trust_item_mutual_class(self, mutual_device):
        """Mutual device should have 'mutual' class."""
        async with TrustItemTestApp(mutual_device).run_test() as pilot:
            item = pilot.app.query_one(TrustItem)
            assert "mutual" in item.classes

    @pytest.mark.asyncio
    async def test_trust_item_offered_class(self, pending_device):
        """Pending device should have 'offered' class."""
        async with TrustItemTestApp(pending_device).run_test() as pilot:
            item = pilot.app.query_one(TrustItem)
            assert "offered" in item.classes

    @pytest.mark.asyncio
    async def test_trust_item_has_revoke_button(self, mutual_device):
        """TrustItem should have a Revoke button."""
        async with TrustItemTestApp(mutual_device).run_test() as pilot:
            buttons = pilot.app.query(Button)
            assert len(list(buttons)) == 1
            button = list(buttons)[0]
            assert "Revoke" in str(button.label)

    @pytest.mark.asyncio
    async def test_trust_item_pending_has_resend_button(self, pending_device):
        """Pending TrustItem should have both Resend and Revoke buttons."""
        async with TrustItemTestApp(pending_device).run_test() as pilot:
            buttons = list(pilot.app.query(Button))
            assert len(buttons) == 2
            labels = [str(b.label) for b in buttons]
            assert any("Resend" in l for l in labels)
            assert any("Revoke" in l for l in labels)

    @pytest.mark.asyncio
    async def test_trust_item_mutual_no_resend_button(self, mutual_device):
        """Mutual TrustItem should NOT have Resend button."""
        async with TrustItemTestApp(mutual_device).run_test() as pilot:
            buttons = list(pilot.app.query(Button))
            assert len(buttons) == 1  # Only Revoke
            labels = [str(b.label) for b in buttons]
            assert not any("Resend" in l for l in labels)

    @pytest.mark.asyncio
    async def test_trust_item_resend_click_posts_message(self, pending_device):
        """Clicking Resend should post ResendClicked message."""
        messages = []

        async with TrustItemTestApp(pending_device).run_test() as pilot:
            item = pilot.app.query_one(TrustItem)

            # Capture messages
            original_post = item.post_message

            def capture_post(msg):
                messages.append(msg)
                return original_post(msg)

            item.post_message = capture_post

            # Find and click the Resend button
            buttons = list(pilot.app.query(Button))
            resend_btn = next(b for b in buttons if "Resend" in str(b.label))
            await pilot.click(resend_btn)

        resend_messages = [m for m in messages if isinstance(m, TrustItem.ResendClicked)]
        assert len(resend_messages) == 1
        assert resend_messages[0].device == pending_device

    @pytest.mark.asyncio
    async def test_trust_item_revoke_click_posts_message(self, pending_device):
        """Clicking Revoke should post RevokeClicked message."""
        messages = []

        async with TrustItemTestApp(pending_device).run_test() as pilot:
            item = pilot.app.query_one(TrustItem)

            original_post = item.post_message

            def capture_post(msg):
                messages.append(msg)
                return original_post(msg)

            item.post_message = capture_post

            # Find and click the Revoke button
            buttons = list(pilot.app.query(Button))
            revoke_btn = next(b for b in buttons if "Revoke" in str(b.label))
            await pilot.click(revoke_btn)

        revoke_messages = [m for m in messages if isinstance(m, TrustItem.RevokeClicked)]
        assert len(revoke_messages) == 1
        assert revoke_messages[0].device == pending_device


class TestTrustViewResend:
    """Tests for TrustView resend functionality."""

    @pytest.fixture
    def pending_device(self):
        return TrustedDevice(
            hash="1112131415161718191a1b1c1d1e1f20",
            name="PendingDevice",
            status=TrustStatus.OFFERED,
            offered_at=1234567890.0,
        )

    @pytest.fixture
    def multiple_pending(self):
        return [
            TrustedDevice(
                hash=f"111213141516171819{i:02x}1b1c1d1e1f20",
                name=f"Pending{i}",
                status=TrustStatus.OFFERED,
                offered_at=1234567890.0 + i,
            )
            for i in range(3)
        ]

    @pytest.mark.asyncio
    async def test_trust_view_shows_resend_for_pending(self, pending_device):
        """TrustView should show Resend button for pending devices."""
        async with TrustViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(TrustView)
            view.update_devices([pending_device])
            await pilot.pause()

            buttons = list(pilot.app.query(Button))
            labels = [str(b.label) for b in buttons]
            assert any("Resend" in l for l in labels)

    @pytest.mark.asyncio
    async def test_trust_view_resend_posts_message(self, pending_device):
        """Clicking Resend in TrustView should post ResendRequested."""
        messages = []

        async with TrustViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(TrustView)
            view.update_devices([pending_device])
            await pilot.pause()

            original_post = view.post_message

            def capture_post(msg):
                messages.append(msg)
                return original_post(msg)

            view.post_message = capture_post

            # Find and click the Resend button
            buttons = list(pilot.app.query(Button))
            resend_btn = next(b for b in buttons if "Resend" in str(b.label))
            await pilot.click(resend_btn)

        resend_messages = [m for m in messages if isinstance(m, TrustView.ResendRequested)]
        assert len(resend_messages) == 1
        assert resend_messages[0].hash_hex == pending_device.hash
        assert resend_messages[0].name == pending_device.name

    @pytest.mark.asyncio
    async def test_trust_view_multiple_pending_resend(self, multiple_pending):
        """Each pending device should have its own Resend button."""
        async with TrustViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(TrustView)
            view.update_devices(multiple_pending)
            await pilot.pause()

            buttons = list(pilot.app.query(Button))
            resend_btns = [b for b in buttons if "Resend" in str(b.label)]
            assert len(resend_btns) == 3  # One per pending device

    @pytest.mark.asyncio
    async def test_trust_view_update_clears_old_buttons(self, pending_device):
        """Updating devices should clear old buttons."""
        async with TrustViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(TrustView)

            # Add pending device
            view.update_devices([pending_device])
            await pilot.pause()
            assert len(list(pilot.app.query(Button))) == 2  # Resend + Revoke

            # Clear devices
            view.update_devices([])
            await pilot.pause()
            assert len(list(pilot.app.query(Button))) == 0


# ============================================================
# TrustView Tests
# ============================================================


class TestTrustView:
    """Tests for the TrustView widget."""

    @pytest.fixture
    def sample_devices(self):
        return [
            TrustedDevice(
                hash="0102030405060708090a0b0c0d0e0f10",
                name="MutualDevice",
                status=TrustStatus.MUTUAL,
                offered_at=1234567890.0,
                accepted_at=1234567900.0,
            ),
            TrustedDevice(
                hash="1112131415161718191a1b1c1d1e1f20",
                name="PendingDevice",
                status=TrustStatus.OFFERED,
                offered_at=1234567890.0,
            ),
        ]

    @pytest.mark.asyncio
    async def test_trust_view_initial_empty(self):
        """TrustView should show empty message initially."""
        async with TrustViewTestApp().run_test() as pilot:
            statics = pilot.app.query(Static)
            empty_msgs = [s for s in statics if "empty" in s.classes]
            assert len(empty_msgs) == 1
            assert "No trusted" in str(empty_msgs[0].render())

    @pytest.mark.asyncio
    async def test_trust_view_update_devices(self, sample_devices):
        """update_devices should display device items."""
        async with TrustViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(TrustView)
            view.update_devices(sample_devices)
            await pilot.pause()

            items = pilot.app.query(TrustItem)
            assert len(list(items)) == 2

    @pytest.mark.asyncio
    async def test_trust_view_sections(self, sample_devices):
        """TrustView should have section headers."""
        async with TrustViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(TrustView)
            view.update_devices(sample_devices)
            await pilot.pause()

            statics = pilot.app.query(Static)
            headers = [s for s in statics if "section-header" in s.classes]
            # Should have "Trusted" and "Pending" headers
            assert len(headers) == 2

    @pytest.mark.asyncio
    async def test_trust_view_empty_after_clear(self, sample_devices):
        """Clearing devices should show empty message."""
        async with TrustViewTestApp().run_test() as pilot:
            view = pilot.app.query_one(TrustView)

            # Add then remove
            view.update_devices(sample_devices)
            await pilot.pause()
            view.update_devices([])
            await pilot.pause()

            statics = pilot.app.query(Static)
            empty_msgs = [s for s in statics if "empty" in s.classes]
            assert len(empty_msgs) == 1


# ============================================================
# CompanionServerApp Tests
# ============================================================


class TestCompanionServerApp:
    """Tests for the main CompanionServerApp."""

    @pytest.fixture
    def mock_rns_service(self):
        """Create a mock RNS service."""
        mock = MagicMock()
        mock.destination_hash = "abc123"
        mock.on_announce = MagicMock()
        mock.on_log = MagicMock()
        return mock

    @pytest.fixture
    def mock_trust_manager(self, tmp_path):
        """Create a mock trust manager."""
        manager = TrustManager(tmp_path)
        return manager

    @pytest.mark.asyncio
    async def test_app_has_announce_panel(self, mock_rns_service, mock_trust_manager):
        """App should have announce panel."""
        app = CompanionServerApp(mock_rns_service, mock_trust_manager)
        async with app.run_test() as pilot:
            view = pilot.app.query_one("#announces", AnnounceView)
            assert view is not None

    @pytest.mark.asyncio
    async def test_app_has_trust_panel(self, mock_rns_service, mock_trust_manager):
        """App should have trust panel."""
        app = CompanionServerApp(mock_rns_service, mock_trust_manager)
        async with app.run_test() as pilot:
            view = pilot.app.query_one("#trusted", TrustView)
            assert view is not None

    @pytest.mark.asyncio
    async def test_app_has_log_panel(self, mock_rns_service, mock_trust_manager):
        """App should have log panel."""
        app = CompanionServerApp(mock_rns_service, mock_trust_manager)
        async with app.run_test() as pilot:
            panel = pilot.app.query_one("#log", LogPanel)
            assert panel is not None

    @pytest.mark.asyncio
    async def test_app_registers_callbacks(self, mock_rns_service, mock_trust_manager):
        """App should register callbacks with rns_service."""
        app = CompanionServerApp(mock_rns_service, mock_trust_manager)
        # Callbacks registered in __init__
        mock_rns_service.on_announce.assert_called_once()
        mock_rns_service.on_log.assert_called_once()

    @pytest.mark.asyncio
    async def test_app_quit_binding(self, mock_rns_service, mock_trust_manager):
        """Pressing 'q' should quit the app."""
        app = CompanionServerApp(mock_rns_service, mock_trust_manager)
        async with app.run_test() as pilot:
            await pilot.press("q")
            # App should exit (this test passes if no exception)

    @pytest.mark.asyncio
    async def test_app_add_announce(self, mock_rns_service, mock_trust_manager):
        """_add_announce should add to the announce view."""
        app = CompanionServerApp(mock_rns_service, mock_trust_manager)
        async with app.run_test() as pilot:
            announce = AnnounceInfo(
                destination_hash=b"\x01" * 16,
                display_name="TestDevice",
                app_data=b"",
                timestamp=1234567890.0,
            )
            app._add_announce(announce)
            await pilot.pause()

            items = pilot.app.query(AnnounceItem)
            assert len(list(items)) == 1

    @pytest.mark.asyncio
    async def test_app_add_log(self, mock_rns_service, mock_trust_manager):
        """_add_log should add to the log panel."""
        app = CompanionServerApp(mock_rns_service, mock_trust_manager)
        async with app.run_test() as pilot:
            panel = pilot.app.query_one("#log", LogPanel)
            initial_count = len(panel._messages)

            app._add_log("Test log message")
            await pilot.pause()

            # Should have one more message
            assert len(panel._messages) == initial_count + 1
            assert "Test log message" in panel._messages[-1]


# ============================================================
# Integration Tests
# ============================================================


class TestTUIIntegration:
    """Integration tests for the TUI."""

    @pytest.fixture
    def mock_rns_service(self):
        mock = MagicMock()
        mock.destination_hash = "abc123"
        mock.on_announce = MagicMock()
        mock.on_log = MagicMock()
        mock.send_trust_offer = MagicMock()
        return mock

    @pytest.fixture
    def mock_trust_manager(self, tmp_path):
        return TrustManager(tmp_path)

    @pytest.mark.asyncio
    async def test_trust_button_calls_send_trust_offer(
        self, mock_rns_service, mock_trust_manager
    ):
        """Clicking trust should call rns_service.send_trust_offer."""
        app = CompanionServerApp(mock_rns_service, mock_trust_manager)

        async with app.run_test() as pilot:
            # Add an announce
            announce = AnnounceInfo(
                destination_hash=b"\x01" * 16,
                display_name="TestDevice",
                app_data=b"",
                timestamp=1234567890.0,
            )
            app._add_announce(announce)
            await pilot.pause()

            # Find the trust button by its ID pattern
            button_id = f"trust-{announce.hash_hex[:8]}"
            button = pilot.app.query_one(f"#{button_id}", Button)

            # Simulate button press directly
            button.press()
            await pilot.pause()

            # Should have called send_trust_offer
            mock_rns_service.send_trust_offer.assert_called_once_with(
                announce.hash_hex
            )
