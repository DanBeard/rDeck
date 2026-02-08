"""Trusted devices view widget."""

from datetime import datetime
from typing import Optional

from textual.app import ComposeResult
from textual.containers import ScrollableContainer
from textual.widgets import Static, Button, Label
from textual.widget import Widget
from textual.message import Message

from ..trust_manager import TrustedDevice, TrustStatus


class TrustItem(Static):
    """A single trusted device item in the list."""

    DEFAULT_CSS = """
    TrustItem {
        layout: horizontal;
        height: auto;
        padding: 1;
        margin: 0 0 1 0;
        background: $surface;
    }

    TrustItem.mutual {
        border-left: thick green;
    }

    TrustItem.offered {
        border-left: thick yellow;
    }

    TrustItem .info {
        width: 1fr;
    }

    TrustItem .name {
        text-style: bold;
    }

    TrustItem .hash {
        color: $text-muted;
    }

    TrustItem .status {
        color: $text-muted;
    }

    TrustItem .status.mutual {
        color: green;
    }

    TrustItem .status.offered {
        color: yellow;
    }

    TrustItem Button {
        width: auto;
        min-width: 8;
    }
    """

    class RevokeClicked(Message):
        """Message sent when revoke button is clicked."""

        def __init__(self, device: TrustedDevice):
            super().__init__()
            self.device = device

    class ResendClicked(Message):
        """Message sent when resend button is clicked."""

        def __init__(self, device: TrustedDevice):
            super().__init__()
            self.device = device

    def __init__(self, device: TrustedDevice, **kwargs):
        super().__init__(**kwargs)
        self.device = device
        self.add_class(device.status.value)

    def compose(self) -> ComposeResult:
        with Static(classes="info"):
            yield Label(self.device.name, classes="name")
            yield Label(self.device.hash[:16] + "...", classes="hash")

            status_text = "Mutual Trust" if self.device.status == TrustStatus.MUTUAL else "Pending"
            status_class = "status " + self.device.status.value
            yield Label(status_text, classes=status_class)

        # Show Resend button for pending devices
        if self.device.status == TrustStatus.OFFERED:
            yield Button("Resend", id=f"resend-{self.device.hash[:8]}", variant="primary")
        yield Button("Revoke", id=f"revoke-{self.device.hash[:8]}")

    def on_button_pressed(self, event: Button.Pressed):
        """Handle button presses."""
        if event.button.id and event.button.id.startswith("resend-"):
            self.post_message(self.ResendClicked(self.device))
        elif event.button.id and event.button.id.startswith("revoke-"):
            self.post_message(self.RevokeClicked(self.device))


class TrustView(Widget):
    """View showing trusted devices."""

    DEFAULT_CSS = """
    TrustView {
        height: 100%;
    }

    TrustView ScrollableContainer {
        height: 100%;
    }

    TrustView .empty {
        padding: 1;
        color: $text-muted;
        text-align: center;
    }

    TrustView .section-header {
        padding: 0 1;
        margin-top: 1;
        text-style: bold;
        color: $text-muted;
    }
    """

    class RevokeRequested(Message):
        """Message sent when user requests to revoke trust."""

        def __init__(self, hash_hex: str):
            super().__init__()
            self.hash_hex = hash_hex

    class ResendRequested(Message):
        """Message sent when user requests to resend a trust offer."""

        def __init__(self, hash_hex: str, name: str):
            super().__init__()
            self.hash_hex = hash_hex
            self.name = name

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._devices: list[TrustedDevice] = []

    def compose(self) -> ComposeResult:
        with ScrollableContainer():
            yield Static("No trusted devices", classes="empty")

    def update_devices(self, devices: list[TrustedDevice]):
        """Update the devices list."""
        self._devices = devices
        self._refresh_list()

    def _refresh_list(self):
        """Refresh the device list."""
        container = self.query_one(ScrollableContainer)

        # Clear existing items - collect first, then remove
        children_to_remove = list(container.children)
        for child in children_to_remove:
            child.remove()

        if not self._devices:
            container.mount(Static("No trusted devices", classes="empty"))
            return

        # Separate mutual and pending
        mutual = [d for d in self._devices if d.status == TrustStatus.MUTUAL]
        pending = [d for d in self._devices if d.status == TrustStatus.OFFERED]

        # Add mutual devices
        if mutual:
            container.mount(Static("Trusted", classes="section-header"))
            for device in mutual:
                container.mount(TrustItem(device))

        # Add pending devices
        if pending:
            container.mount(Static("Pending", classes="section-header"))
            for device in pending:
                container.mount(TrustItem(device))

    def on_trust_item_revoke_clicked(self, message: TrustItem.RevokeClicked):
        """Handle revoke click from trust item."""
        self.post_message(self.RevokeRequested(message.device.hash))

    def on_trust_item_resend_clicked(self, message: TrustItem.ResendClicked):
        """Handle resend click from trust item."""
        self.post_message(self.ResendRequested(message.device.hash, message.device.name))
