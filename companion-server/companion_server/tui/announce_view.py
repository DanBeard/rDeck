"""Announce stream view widget."""

from datetime import datetime
from typing import Optional

from textual.app import ComposeResult
from textual.containers import ScrollableContainer
from textual.widgets import Static, Button, Label
from textual.widget import Widget
from textual.message import Message

from ..reticulum_service import AnnounceInfo


class AnnounceItem(Static):
    """A single announce item in the list."""

    DEFAULT_CSS = """
    AnnounceItem {
        layout: horizontal;
        height: auto;
        padding: 1;
        margin: 0 0 1 0;
        background: $surface;
    }

    AnnounceItem:hover {
        background: $surface-lighten-1;
    }

    AnnounceItem .info {
        width: 1fr;
    }

    AnnounceItem .name {
        text-style: bold;
    }

    AnnounceItem .hash {
        color: $text-muted;
    }

    AnnounceItem .time {
        color: $text-muted;
    }

    AnnounceItem Button {
        width: auto;
        min-width: 8;
    }
    """

    class TrustClicked(Message):
        """Message sent when trust button is clicked."""

        def __init__(self, announce: AnnounceInfo):
            super().__init__()
            self.announce = announce

    def __init__(self, announce: AnnounceInfo, **kwargs):
        super().__init__(**kwargs)
        self.announce = announce

    def compose(self) -> ComposeResult:
        with Static(classes="info"):
            yield Label(self.announce.display_name, classes="name")
            yield Label(self.announce.hash_hex[:16] + "...", classes="hash")
            time_str = datetime.fromtimestamp(self.announce.timestamp).strftime("%H:%M:%S")
            yield Label(time_str, classes="time")

        yield Button("Trust", id=f"trust-{self.announce.hash_hex[:8]}")

    def on_button_pressed(self, event: Button.Pressed):
        """Handle trust button press."""
        self.post_message(self.TrustClicked(self.announce))


class AnnounceView(Widget):
    """View showing recent announces with trust buttons."""

    DEFAULT_CSS = """
    AnnounceView {
        height: 100%;
    }

    AnnounceView ScrollableContainer {
        height: 100%;
    }

    AnnounceView .empty {
        padding: 1;
        color: $text-muted;
        text-align: center;
    }
    """

    class TrustRequested(Message):
        """Message sent when user requests to trust a device."""

        def __init__(self, hash_hex: str, name: str):
            super().__init__()
            self.hash_hex = hash_hex
            self.name = name

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._announces: dict[str, AnnounceInfo] = {}

    def compose(self) -> ComposeResult:
        with ScrollableContainer():
            yield Static("Waiting for announces...", classes="empty", id="empty-msg")

    def add_announce(self, announce: AnnounceInfo):
        """Add or update an announce."""
        self._announces[announce.hash_hex] = announce
        self._refresh_list()

    def _refresh_list(self):
        """Refresh the announce list."""
        container = self.query_one(ScrollableContainer)

        # Clear existing items
        for child in list(container.children):
            child.remove()

        if not self._announces:
            container.mount(Static("Waiting for announces...", classes="empty", id="empty-msg"))
            return

        # Sort by timestamp (newest first)
        sorted_announces = sorted(
            self._announces.values(),
            key=lambda a: a.timestamp,
            reverse=True,
        )

        # Add items
        for announce in sorted_announces[:20]:  # Limit to 20
            container.mount(AnnounceItem(announce))

    def on_announce_item_trust_clicked(self, message: AnnounceItem.TrustClicked):
        """Handle trust click from announce item."""
        self.post_message(
            self.TrustRequested(
                message.announce.hash_hex,
                message.announce.display_name,
            )
        )
