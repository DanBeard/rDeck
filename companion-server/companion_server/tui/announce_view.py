"""Announce stream view widget."""

from datetime import datetime
from typing import Optional

from textual.app import ComposeResult
from textual.containers import ScrollableContainer, Vertical, Horizontal
from textual.widgets import Static, Button, Label, Input, Checkbox
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

    AnnounceItem .device-type {
        color: $success;
        text-style: bold;
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
            # Show device type badge if known
            if self.announce.device_type:
                type_label = self.announce.device_type
                if self.announce.services:
                    type_label += f" ({', '.join(self.announce.services)})"
                yield Label(f"[{type_label}]", classes="device-type")
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
        layout: vertical;
    }

    AnnounceView #filter-container {
        height: auto;
        max-height: 5;
        padding: 0 1;
    }

    AnnounceView #filter-row {
        height: 3;
        width: 100%;
    }

    AnnounceView #filter-input {
        width: 1fr;
        height: 3;
    }

    AnnounceView #known-only-checkbox {
        width: auto;
        margin-left: 1;
    }

    AnnounceView #filter-status {
        height: 1;
        color: $text-muted;
        text-align: right;
        margin-bottom: 1;
    }

    AnnounceView ScrollableContainer {
        height: 1fr;
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
        self._filter_text: str = ""
        self._known_only: bool = False

    def compose(self) -> ComposeResult:
        with Vertical(id="filter-container"):
            with Horizontal(id="filter-row"):
                yield Input(placeholder="Filter by name or address...", id="filter-input")
                yield Checkbox("Known only", id="known-only-checkbox")
            yield Static("", id="filter-status")
        with ScrollableContainer():
            yield Static("Waiting for announces...", classes="empty")

    def on_input_changed(self, event: Input.Changed) -> None:
        """Handle filter input changes."""
        if event.input.id == "filter-input":
            self._filter_text = event.value.lower()
            self._refresh_list()

    def on_checkbox_changed(self, event: Checkbox.Changed) -> None:
        """Handle checkbox changes."""
        if event.checkbox.id == "known-only-checkbox":
            self._known_only = event.value
            self._refresh_list()

    def add_announce(self, announce: AnnounceInfo):
        """Add or update an announce."""
        self._announces[announce.hash_hex] = announce
        self.log.info(f"Added announce: {announce.display_name} (total: {len(self._announces)})")
        self._refresh_list()

    def _matches_filter(self, announce: AnnounceInfo) -> bool:
        """Check if an announce matches the current filter."""
        # Check known-only filter
        if self._known_only and not announce.is_known_type:
            return False

        # Check text filter
        if self._filter_text:
            text_match = (
                self._filter_text in announce.display_name.lower()
                or self._filter_text in announce.hash_hex.lower()
                or (announce.device_type and self._filter_text in announce.device_type.lower())
            )
            if not text_match:
                return False

        return True

    def _refresh_list(self):
        """Refresh the announce list."""
        try:
            container = self.query_one(ScrollableContainer)
            status = self.query_one("#filter-status", Static)
        except Exception as e:
            self.log.error(f"Error querying widgets in _refresh_list: {e}")
            return

        # Clear existing items - collect first, then remove
        children_to_remove = list(container.children)
        for child in children_to_remove:
            child.remove()

        if not self._announces:
            container.mount(Static("Waiting for announces...", classes="empty"))
            status.update("")
            return

        # Filter and sort by timestamp (newest first)
        filtered_announces = [a for a in self._announces.values() if self._matches_filter(a)]
        sorted_announces = sorted(
            filtered_announces,
            key=lambda a: a.timestamp,
            reverse=True,
        )

        # Update status
        total = len(self._announces)
        shown = len(sorted_announces[:20])
        if self._filter_text:
            status.update(f"Showing {shown} of {len(filtered_announces)} matches ({total} total)")
        else:
            status.update(f"Showing {shown} of {total}")

        if not sorted_announces:
            if self._filter_text or self._known_only:
                msg = "No matches"
                if self._filter_text:
                    msg += f" for '{self._filter_text}'"
                if self._known_only:
                    msg += " (known only filter active)"
                msg += f" - {total} total announces"
                container.mount(Static(msg, classes="empty"))
            else:
                container.mount(Static("Waiting for announces...", classes="empty"))
            return

        # Add items
        for announce in sorted_announces[:20]:  # Limit to 20
            container.mount(AnnounceItem(announce))

        # Force refresh
        self.refresh()

    def focus_filter(self):
        """Focus the filter input."""
        self.query_one("#filter-input", Input).focus()

    def clear_filter(self):
        """Clear the filter."""
        input_widget = self.query_one("#filter-input", Input)
        input_widget.value = ""
        self._filter_text = ""
        self._refresh_list()

    def on_announce_item_trust_clicked(self, message: AnnounceItem.TrustClicked):
        """Handle trust click from announce item."""
        self.post_message(
            self.TrustRequested(
                message.announce.hash_hex,
                message.announce.display_name,
            )
        )
