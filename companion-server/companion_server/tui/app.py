"""Main Textual TUI application."""

import re
import socket
import threading
import time
from datetime import datetime
from pathlib import Path

from textual.app import App, ComposeResult
from textual.containers import Container, Horizontal, Vertical, ScrollableContainer
from textual.widgets import Header, Footer, Static, Button, Label, ListView, ListItem, TabbedContent, TabPane
from textual.binding import Binding
from textual.message import Message

from ..reticulum_service import ReticulumService, AnnounceInfo, ServiceEvent
from ..trust_manager import TrustManager, TrustStatus
from ..config import Config
from .announce_view import AnnounceView
from .trust_view import TrustView
from .service_views import NTPServiceView, SearchServiceView, MapsServiceView, PropagationServiceView


def _get_local_ip() -> str:
    """Get the local network IP address (not 127.0.0.1)."""
    try:
        # Connect to a public IP (doesn't actually send data) to determine
        # which local interface would be used for outbound traffic.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("10.255.255.255", 1))
            return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"


def _get_tcp_port(data_dir: Path) -> int | None:
    """Read the TCP listen port from the Reticulum config, if present."""
    config_path = data_dir / "reticulum" / "config"
    if not config_path.exists():
        return None
    try:
        text = config_path.read_text()
        if "TCPServerInterface" not in text:
            return None
        m = re.search(r"listen_port\s*=\s*(\d+)", text)
        return int(m.group(1)) if m else None
    except OSError:
        return None


class TrustStateChanged(Message):
    """Message posted when trust state changes (from background thread)."""
    pass


class LogMessage(Message):
    """Message posted to add a log entry (from any thread)."""
    def __init__(self, text: str):
        super().__init__()
        self.text = text


class AnnounceReceived(Message):
    """Message posted when an announce is received (from background thread)."""
    def __init__(self, announce: AnnounceInfo):
        super().__init__()
        self.announce = announce


class ServiceEventReceived(Message):
    """Message posted when a service event is received (from background thread)."""
    def __init__(self, event: ServiceEvent):
        super().__init__()
        self.event = event


class LogPanel(Static):
    """Log message display panel."""

    DEFAULT_CSS = """
    LogPanel {
        height: 100%;
        padding: 0 1;
    }
    """

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._messages: list[str] = []
        self._max_messages = 100

    def add_log(self, message: str):
        """Add a log message."""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self._messages.append(f"[{timestamp}] {message}")
        if len(self._messages) > self._max_messages:
            self._messages = self._messages[-self._max_messages:]
        self.update("\n".join(self._messages[-50:]))  # Show last 50


class CompanionServerApp(App):
    """Companion Server TUI application."""

    TITLE = "Companion Server"

    CSS = """
    Screen {
        layout: grid;
        grid-size: 2 2;
        grid-columns: 1fr 1fr;
        grid-rows: 1fr 1fr;
    }

    #announce-panel {
        column-span: 1;
        row-span: 1;
    }

    #trust-panel {
        column-span: 1;
        row-span: 1;
    }

    #tabs-panel {
        column-span: 2;
        row-span: 1;
    }

    .panel-title {
        text-style: bold;
        color: cyan;
        padding: 0 1;
    }

    AnnounceView {
        height: 100%;
        border: solid blue;
    }

    TrustView {
        height: 100%;
        border: solid yellow;
    }

    #service-tabs {
        height: 100%;
    }

    #service-tabs > ContentSwitcher {
        height: 1fr;
    }

    TabPane {
        height: 100%;
        padding: 0;
    }
    """

    BINDINGS = [
        Binding("q", "quit", "Quit"),
        Binding("r", "refresh", "Refresh"),
        Binding("a", "announce", "Announce"),
        Binding("/", "filter", "Filter"),
        Binding("escape", "clear_filter", "Clear Filter", show=False),
        Binding("c", "copy_log", "Copy Log"),
    ]

    def __init__(self, rns_service: ReticulumService, trust_manager: TrustManager):
        super().__init__()
        self.rns_service = rns_service
        self.trust_manager = trust_manager

        # Show local IP and TCP port in header for easy device configuration
        local_ip = _get_local_ip()
        tcp_port = _get_tcp_port(rns_service.config.data_dir)
        if tcp_port is not None:
            self.sub_title = f"{local_ip}:{tcp_port}"
        else:
            self.sub_title = local_ip

        # Register callbacks
        self.rns_service.on_announce(self._on_announce)
        self.rns_service.on_log(self._on_log)
        self.rns_service.on_service_event(self._on_service_event)
        self.trust_manager.on_change(self._on_trust_change)

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)

        with Container(id="announce-panel"):
            yield Label("Announces", classes="panel-title")
            yield AnnounceView(id="announces")

        with Container(id="trust-panel"):
            yield Label("Trusted Devices", classes="panel-title")
            yield TrustView(id="trusted")

        with Container(id="tabs-panel"):
            with TabbedContent(id="service-tabs"):
                with TabPane("Log", id="tab-log"):
                    yield LogPanel(id="log")
                with TabPane("NTP", id="tab-ntp"):
                    yield NTPServiceView(id="ntp-view")
                with TabPane("Search", id="tab-search"):
                    yield SearchServiceView(self.rns_service.config, id="search-view")
                with TabPane("Maps", id="tab-maps"):
                    yield MapsServiceView(
                        self.rns_service.config,
                        maps_service=self.rns_service._maps_service,
                        id="maps-view",
                    )
                with TabPane("Propagation", id="tab-propagation"):
                    yield PropagationServiceView(
                        self.rns_service.config,
                        id="propagation-view",
                    )

        yield Footer()

    def on_mount(self):
        """Called when app is mounted."""
        self._on_log("Companion Server started")
        if self.rns_service.destination_hash:
            self._on_log(f"LXMF address: {self.rns_service.destination_hash}")

        # Update trust view with initial state
        self._update_trust_view()

    def _on_announce(self, announce: AnnounceInfo):
        """Handle new announce from Reticulum service."""
        self.post_message(AnnounceReceived(announce))

    def on_announce_received(self, message: AnnounceReceived):
        """Handle announce received message (runs in main thread)."""
        self._add_announce(message.announce)

    def _add_announce(self, announce: AnnounceInfo):
        """Add announce to view (must be called from main thread)."""
        try:
            announce_view = self.query_one("#announces", AnnounceView)
            announce_view.add_announce(announce)
            self._add_log(f"Added to view: {announce.display_name} (type={announce.device_type})")
        except Exception as e:
            self._add_log(f"Error adding announce: {e}")

    def _on_log(self, message: str):
        """Handle log message from Reticulum service."""
        self.post_message(LogMessage(message))

    def on_log_message(self, message: LogMessage):
        """Handle log message (runs in main thread)."""
        self._add_log(message.text)

    def _add_log(self, message: str):
        """Add log message (must be called from main thread)."""
        log_panel = self.query_one("#log", LogPanel)
        log_panel.add_log(message)

    def _on_service_event(self, event: ServiceEvent):
        """Handle service event from background thread."""
        self.post_message(ServiceEventReceived(event))

    def on_service_event_received(self, message: ServiceEventReceived):
        """Route service event to the appropriate tab (runs in main thread)."""
        event = message.event
        try:
            if event.service == "ntp":
                view = self.query_one("#ntp-view", NTPServiceView)
                view.add_event(event)
            elif event.service == "search":
                view = self.query_one("#search-view", SearchServiceView)
                view.add_event(event)
            elif event.service == "maps":
                view = self.query_one("#maps-view", MapsServiceView)
                view.add_event(event)
            elif event.service == "propagation":
                view = self.query_one("#propagation-view", PropagationServiceView)
                view.add_event(event)
        except Exception as e:
            self._add_log(f"Error routing service event: {e}")

    def _on_trust_change(self):
        """Handle trust state change."""
        self.post_message(TrustStateChanged())

    def on_trust_state_changed(self, message: TrustStateChanged):
        """Handle trust state changed message (runs in main thread)."""
        self._update_trust_view()

    def _update_trust_view(self):
        """Update trust view with current state."""
        trust_view = self.query_one("#trusted", TrustView)
        trust_view.update_devices(self.trust_manager.get_all_devices())

    def on_announce_view_trust_requested(self, message: AnnounceView.TrustRequested):
        """Handle trust request from announce view."""
        self._add_log(f"Sending trust offer to {message.name}...")
        hash_hex = message.hash_hex
        name = message.name

        def send_offer():
            try:
                self.rns_service.send_trust_offer(hash_hex)
                self.call_from_thread(lambda: self.notify(f"Trust offer sent to {name}"))
            except Exception as e:
                self.call_from_thread(lambda: self.notify(f"Error: {e}", severity="error"))

        thread = threading.Thread(target=send_offer, daemon=True)
        thread.start()

    def on_trust_view_resend_requested(self, message: TrustView.ResendRequested):
        """Handle resend request from trust view."""
        self._add_log(f"Resending trust offer to {message.name}...")
        hash_hex = message.hash_hex
        name = message.name

        def send_offer():
            try:
                self.rns_service.send_trust_offer(hash_hex)
                self.call_from_thread(lambda: self.notify(f"Trust offer resent to {name}"))
            except Exception as e:
                self.call_from_thread(lambda: self.notify(f"Error: {e}", severity="error"))

        thread = threading.Thread(target=send_offer, daemon=True)
        thread.start()

    def on_maps_service_view_config_changed(self, message: MapsServiceView.ConfigChanged):
        """Handle maps config changes from the maps tab."""
        self._add_log(f"Maps config updated: {message.field} = {message.value}")

    def action_quit(self):
        """Quit the application."""
        self.exit()

    def action_refresh(self):
        """Refresh views."""
        self._update_trust_view()

    def action_announce(self):
        """Send an announce."""
        self._add_log("Sending announce...")

        def do_announce():
            try:
                self.rns_service._announce()
                self.call_from_thread(lambda: self.notify("Announce sent"))
            except Exception as e:
                self.call_from_thread(lambda: self.notify(f"Error: {e}", severity="error"))

        thread = threading.Thread(target=do_announce, daemon=True)
        thread.start()

    def action_filter(self):
        """Focus the announce filter input."""
        announce_view = self.query_one("#announces", AnnounceView)
        announce_view.focus_filter()

    def action_clear_filter(self):
        """Clear the announce filter."""
        announce_view = self.query_one("#announces", AnnounceView)
        announce_view.clear_filter()

    def action_copy_log(self):
        """Copy log contents to clipboard."""
        log_panel = self.query_one("#log", LogPanel)
        log_text = "\n".join(log_panel._messages)
        self.copy_to_clipboard(log_text)
        self._add_log("Log copied to clipboard")
