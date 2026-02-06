"""Service tab views for the companion server TUI."""

import threading
import time
import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Optional

from textual.app import ComposeResult
from textual.containers import ScrollableContainer, Horizontal, Vertical
from textual.widgets import Static, Button, Label, Input
from textual.widget import Widget
from textual.message import Message

from ..reticulum_service import ServiceEvent
from ..config import Config


class BaseServiceView(Widget):
    """Base widget for service tab views.

    Provides a common layout: status section at top, optional buttons row,
    and a scrollable request history at bottom.
    """

    DEFAULT_CSS = """
    BaseServiceView {
        height: 100%;
        layout: vertical;
    }

    BaseServiceView .status-section {
        height: auto;
        max-height: 8;
        padding: 0 1;
        border-bottom: solid $surface-lighten-2;
    }

    BaseServiceView .status-row {
        height: auto;
        margin: 0;
    }

    BaseServiceView .status-label {
        color: $text-muted;
        width: auto;
        margin-right: 2;
    }

    BaseServiceView .status-value {
        width: auto;
    }

    BaseServiceView .status-value.ok {
        color: green;
    }

    BaseServiceView .status-value.warn {
        color: yellow;
    }

    BaseServiceView .status-value.error {
        color: red;
    }

    BaseServiceView .buttons-row {
        height: auto;
        padding: 1;
        border-bottom: solid $surface-lighten-2;
    }

    BaseServiceView .buttons-row Button {
        margin-right: 1;
    }

    BaseServiceView .history-section {
        height: 1fr;
    }

    BaseServiceView .history-header {
        text-style: bold;
        color: $text-muted;
        padding: 0 1;
    }

    BaseServiceView .history-entry {
        padding: 0 1;
        height: auto;
    }

    BaseServiceView .history-entry.error {
        color: red;
    }

    BaseServiceView .empty-history {
        color: $text-muted;
        padding: 1;
        text-align: center;
    }
    """

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._events: list[ServiceEvent] = []
        self._max_events = 100
        self._request_count = 0

    def add_event(self, event: ServiceEvent):
        """Add a service event to the history."""
        self._events.append(event)
        self._request_count += 1
        if len(self._events) > self._max_events:
            self._events = self._events[-self._max_events:]
        self._refresh_history()

    def _refresh_history(self):
        """Refresh the history display. Override in subclasses for custom formatting."""
        try:
            container = self.query_one(".history-section", ScrollableContainer)
        except Exception:
            return

        children_to_remove = list(container.children)
        for child in children_to_remove:
            child.remove()

        if not self._events:
            container.mount(Static("No requests yet", classes="empty-history"))
            return

        # Show most recent events first, limit to 50
        for event in reversed(self._events[-50:]):
            ts = datetime.fromtimestamp(event.timestamp).strftime("%H:%M:%S")
            css_class = "history-entry"
            if event.event_type == "error":
                css_class += " error"
            container.mount(Static(f"[{ts}] {event.details}", classes=css_class))


class NTPServiceView(BaseServiceView):
    """NTP service status and request history."""

    DEFAULT_CSS = BaseServiceView.DEFAULT_CSS + """
    NTPServiceView {
        height: 100%;
    }
    """

    def compose(self) -> ComposeResult:
        with Vertical(classes="status-section"):
            with Horizontal(classes="status-row"):
                yield Static("Status: ", classes="status-label")
                yield Static("Enabled", classes="status-value ok", id="ntp-status")
            with Horizontal(classes="status-row"):
                yield Static("Requests served: ", classes="status-label")
                yield Static("0", classes="status-value", id="ntp-count")
        yield Static("Request History", classes="history-header")
        with ScrollableContainer(classes="history-section"):
            yield Static("No requests yet", classes="empty-history")

    def add_event(self, event: ServiceEvent):
        """Add event and update stats."""
        super().add_event(event)
        try:
            count_label = self.query_one("#ntp-count", Static)
            count_label.update(str(self._request_count))
        except Exception:
            pass


class SearchServiceView(BaseServiceView):
    """Search service status and query history."""

    DEFAULT_CSS = BaseServiceView.DEFAULT_CSS + """
    SearchServiceView {
        height: 100%;
    }
    """

    def __init__(self, config: Config, **kwargs):
        super().__init__(**kwargs)
        self._config = config

    def compose(self) -> ComposeResult:
        ai_status = "Yes" if self._config.ai_summary_enabled else "No"
        ai_class = "status-value ok" if self._config.ai_summary_enabled else "status-value"

        with Vertical(classes="status-section"):
            with Horizontal(classes="status-row"):
                yield Static("Status: ", classes="status-label")
                yield Static("Enabled", classes="status-value ok", id="search-status")
            with Horizontal(classes="status-row"):
                yield Static("AI Summary: ", classes="status-label")
                yield Static(ai_status, classes=ai_class, id="search-ai")
            with Horizontal(classes="status-row"):
                yield Static("Max results: ", classes="status-label")
                yield Static(str(self._config.search_max_results), classes="status-value", id="search-max")
            with Horizontal(classes="status-row"):
                yield Static("Total searches: ", classes="status-label")
                yield Static("0", classes="status-value", id="search-count")
        yield Static("Query History", classes="history-header")
        with ScrollableContainer(classes="history-section"):
            yield Static("No queries yet", classes="empty-history")

    def add_event(self, event: ServiceEvent):
        """Add event and update stats."""
        super().add_event(event)
        try:
            count_label = self.query_one("#search-count", Static)
            count_label.update(str(self._request_count))
        except Exception:
            pass


class MapsServiceView(BaseServiceView):
    """Maps service status, configuration helpers, and request history."""

    DEFAULT_CSS = BaseServiceView.DEFAULT_CSS + """
    MapsServiceView {
        height: 100%;
    }

    MapsServiceView .config-section {
        height: auto;
        padding: 1;
        border-bottom: solid $surface-lighten-2;
    }

    MapsServiceView .config-section Input {
        margin: 0 1 0 0;
        width: 1fr;
    }

    MapsServiceView .config-row {
        height: auto;
        margin-bottom: 1;
    }

    MapsServiceView .config-label {
        width: auto;
        min-width: 16;
        margin-right: 1;
        color: $text-muted;
    }

    MapsServiceView .test-result {
        height: auto;
        padding: 0 1;
    }

    MapsServiceView .test-result.ok {
        color: green;
    }

    MapsServiceView .test-result.fail {
        color: red;
    }
    """

    class ConfigChanged(Message):
        """Posted when maps config is changed via the TUI."""
        def __init__(self, field: str, value: str):
            super().__init__()
            self.field = field
            self.value = value

    def __init__(self, config: Config, maps_service=None, **kwargs):
        super().__init__(**kwargs)
        self._config = config
        self._maps_service = maps_service

    def compose(self) -> ComposeResult:
        # Status indicators
        mbtiles_path = self._config.maps_mbtiles_path
        mbtiles_status = mbtiles_path if mbtiles_path else "Not configured"
        mbtiles_class = "status-value ok" if mbtiles_path and Path(mbtiles_path).exists() else "status-value warn"

        tileserver_url = self._config.maps_tileserver_url
        tileserver_status = tileserver_url if tileserver_url else "Not configured"
        tileserver_class = "status-value ok" if tileserver_url else "status-value warn"

        valhalla_url = self._config.maps_valhalla_url
        valhalla_status = valhalla_url if valhalla_url else "Not configured"
        valhalla_class = "status-value ok" if valhalla_url else "status-value warn"

        nominatim_url = self._config.maps_nominatim_url
        nominatim_status = nominatim_url if nominatim_url else "Not configured"
        nominatim_class = "status-value ok" if nominatim_url else "status-value warn"

        try:
            from PIL import Image
            pil_available = True
        except ImportError:
            pil_available = False
        pil_class = "status-value ok" if pil_available else "status-value error"

        with Vertical(classes="status-section"):
            with Horizontal(classes="status-row"):
                yield Static("Tileserver: ", classes="status-label")
                yield Static(tileserver_status, classes=tileserver_class, id="maps-tileserver-status")
            with Horizontal(classes="status-row"):
                yield Static("MBTiles: ", classes="status-label")
                yield Static(mbtiles_status, classes=mbtiles_class, id="maps-mbtiles-status")
            with Horizontal(classes="status-row"):
                yield Static("Valhalla: ", classes="status-label")
                yield Static(valhalla_status, classes=valhalla_class, id="maps-valhalla-status")
            with Horizontal(classes="status-row"):
                yield Static("Nominatim: ", classes="status-label")
                yield Static(nominatim_status, classes=nominatim_class, id="maps-nominatim-status")
            with Horizontal(classes="status-row"):
                yield Static("PIL: ", classes="status-label")
                yield Static("Available" if pil_available else "Not installed", classes=pil_class, id="maps-pil-status")
            with Horizontal(classes="status-row"):
                yield Static("Requests: ", classes="status-label")
                yield Static("0", classes="status-value", id="maps-count")

        # Config buttons
        with Horizontal(classes="buttons-row"):
            yield Button("Set Tileserver URL", id="maps-set-tileserver")
            yield Button("Set MBTiles Path", id="maps-set-mbtiles")
            yield Button("Set Valhalla URL", id="maps-set-valhalla")
            yield Button("Set Nominatim URL", id="maps-set-nominatim")
            yield Button("Test Services", id="maps-test")

        # Inline config input (hidden by default, shown when a button is clicked)
        with Vertical(classes="config-section", id="maps-config-input"):
            with Horizontal(classes="config-row"):
                yield Static("", classes="config-label", id="maps-config-prompt")
                yield Input(placeholder="Enter value...", id="maps-config-value")
                yield Button("Save", id="maps-config-save", variant="primary")
                yield Button("Cancel", id="maps-config-cancel")
            yield Static("", id="maps-test-results", classes="test-result")

        yield Static("Request History", classes="history-header")
        with ScrollableContainer(classes="history-section"):
            yield Static("No requests yet", classes="empty-history")

    def on_mount(self):
        """Hide config input section initially."""
        config_section = self.query_one("#maps-config-input")
        config_section.display = False
        self._active_config_field: Optional[str] = None

    def on_button_pressed(self, event: Button.Pressed):
        """Handle button presses."""
        button_id = event.button.id
        if not button_id:
            return

        config_section = self.query_one("#maps-config-input")
        prompt_label = self.query_one("#maps-config-prompt", Static)
        input_widget = self.query_one("#maps-config-value", Input)
        test_results = self.query_one("#maps-test-results", Static)
        test_results.update("")

        if button_id == "maps-set-tileserver":
            self._active_config_field = "maps_tileserver_url"
            prompt_label.update("Tileserver URL:")
            input_widget.value = self._config.maps_tileserver_url or ""
            input_widget.placeholder = "http://localhost:8081"
            config_section.display = True
            input_widget.focus()

        elif button_id == "maps-set-mbtiles":
            self._active_config_field = "maps_mbtiles_path"
            prompt_label.update("MBTiles path:")
            input_widget.value = self._config.maps_mbtiles_path or ""
            input_widget.placeholder = "/path/to/tiles.mbtiles"
            config_section.display = True
            input_widget.focus()

        elif button_id == "maps-set-valhalla":
            self._active_config_field = "maps_valhalla_url"
            prompt_label.update("Valhalla URL:")
            input_widget.value = self._config.maps_valhalla_url or ""
            input_widget.placeholder = "http://localhost:8002"
            config_section.display = True
            input_widget.focus()

        elif button_id == "maps-set-nominatim":
            self._active_config_field = "maps_nominatim_url"
            prompt_label.update("Nominatim URL:")
            input_widget.value = self._config.maps_nominatim_url or ""
            input_widget.placeholder = "http://localhost:8080"
            config_section.display = True
            input_widget.focus()

        elif button_id == "maps-config-save":
            if self._active_config_field:
                value = input_widget.value.strip()
                self._apply_config(self._active_config_field, value)
                config_section.display = False
                self._active_config_field = None

        elif button_id == "maps-config-cancel":
            config_section.display = False
            self._active_config_field = None

        elif button_id == "maps-test":
            self._run_service_tests()

    def on_input_submitted(self, event: Input.Submitted):
        """Handle Enter in config input."""
        if event.input.id == "maps-config-value" and self._active_config_field:
            value = event.value.strip()
            self._apply_config(self._active_config_field, value)
            config_section = self.query_one("#maps-config-input")
            config_section.display = False
            self._active_config_field = None

    def _apply_config(self, field: str, value: str):
        """Apply a config change and update status indicators."""
        if field == "maps_tileserver_url":
            self._config.maps_tileserver_url = value or None
            self._config.save()
            if self._maps_service:
                self._maps_service.reload_config(self._config)
            self._update_tileserver_status()
            self.notify("Tileserver URL updated")

        elif field == "maps_mbtiles_path":
            if value and not Path(value).exists():
                self.notify(f"File not found: {value}", severity="warning")
                return
            self._config.maps_mbtiles_path = value or None
            self._config.save()
            # Hot-reload MBTiles
            if self._maps_service and value:
                self._maps_service.reload_mbtiles(value)
            self._update_mbtiles_status()
            self.notify("MBTiles path updated")

        elif field == "maps_valhalla_url":
            self._config.maps_valhalla_url = value or None
            self._config.save()
            self._update_valhalla_status()
            self.notify("Valhalla URL updated")

        elif field == "maps_nominatim_url":
            self._config.maps_nominatim_url = value or None
            self._config.save()
            self._update_nominatim_status()
            self.notify("Nominatim URL updated")

        self.post_message(self.ConfigChanged(field, value))

    def _update_mbtiles_status(self):
        """Update the MBTiles status indicator."""
        try:
            status = self.query_one("#maps-mbtiles-status", Static)
            path = self._config.maps_mbtiles_path
            if path and Path(path).exists():
                status.update(path)
                status.remove_class("warn")
                status.add_class("ok")
            elif path:
                status.update(f"{path} (not found)")
                status.remove_class("ok")
                status.add_class("warn")
            else:
                status.update("Not configured")
                status.remove_class("ok")
                status.add_class("warn")
        except Exception:
            pass

    def _update_valhalla_status(self):
        """Update the Valhalla status indicator."""
        try:
            status = self.query_one("#maps-valhalla-status", Static)
            url = self._config.maps_valhalla_url
            if url:
                status.update(url)
                status.remove_class("warn")
                status.add_class("ok")
            else:
                status.update("Not configured")
                status.remove_class("ok")
                status.add_class("warn")
        except Exception:
            pass

    def _update_nominatim_status(self):
        """Update the Nominatim status indicator."""
        try:
            status = self.query_one("#maps-nominatim-status", Static)
            url = self._config.maps_nominatim_url
            if url:
                status.update(url)
                status.remove_class("warn")
                status.add_class("ok")
            else:
                status.update("Not configured")
                status.remove_class("ok")
                status.add_class("warn")
        except Exception:
            pass

    def _update_tileserver_status(self):
        """Update the Tileserver status indicator."""
        try:
            status = self.query_one("#maps-tileserver-status", Static)
            url = self._config.maps_tileserver_url
            if url:
                status.update(url)
                status.remove_class("warn")
                status.add_class("ok")
            else:
                status.update("Not configured")
                status.remove_class("ok")
                status.add_class("warn")
        except Exception:
            pass

    def _run_service_tests(self):
        """Test connectivity to configured services."""
        test_results = self.query_one("#maps-test-results", Static)
        config_section = self.query_one("#maps-config-input")
        config_section.display = True
        prompt_label = self.query_one("#maps-config-prompt", Static)
        prompt_label.update("")
        input_widget = self.query_one("#maps-config-value", Input)
        input_widget.display = False
        save_btn = self.query_one("#maps-config-save", Button)
        save_btn.display = False

        test_results.update("Testing services...")
        self._active_config_field = None

        def run_tests():
            results = []

            # Test Tileserver
            tileserver_url = self._config.maps_tileserver_url
            if tileserver_url:
                try:
                    import httpx
                    resp = httpx.get(f"{tileserver_url}/health", timeout=5.0)
                    if resp.status_code == 200:
                        results.append(f"  Tileserver: OK")
                    else:
                        # Try fetching the styles index as fallback health check
                        resp = httpx.get(f"{tileserver_url}/styles.json", timeout=5.0)
                        results.append(f"  Tileserver: OK (status {resp.status_code})")
                except Exception as e:
                    results.append(f"  Tileserver: FAIL ({e})")
            else:
                results.append(f"  Tileserver: Not configured")

            # Test MBTiles
            mbtiles_path = self._config.maps_mbtiles_path
            if mbtiles_path and Path(mbtiles_path).exists():
                try:
                    conn = sqlite3.connect(mbtiles_path)
                    cursor = conn.execute("SELECT count(*) FROM tiles")
                    count = cursor.fetchone()[0]
                    conn.close()
                    results.append(f"  MBTiles: OK ({count} tiles)")
                except Exception as e:
                    results.append(f"  MBTiles: FAIL ({e})")
            else:
                results.append(f"  MBTiles: Not configured")

            # Test Valhalla
            valhalla_url = self._config.maps_valhalla_url
            if valhalla_url:
                try:
                    import httpx
                    resp = httpx.get(f"{valhalla_url}/status", timeout=5.0)
                    results.append(f"  Valhalla: OK (status {resp.status_code})")
                except Exception as e:
                    results.append(f"  Valhalla: FAIL ({e})")
            else:
                results.append(f"  Valhalla: Not configured")

            # Test Nominatim
            nominatim_url = self._config.maps_nominatim_url
            if nominatim_url:
                try:
                    import httpx
                    resp = httpx.get(f"{nominatim_url}/status", timeout=5.0)
                    results.append(f"  Nominatim: OK (status {resp.status_code})")
                except Exception as e:
                    results.append(f"  Nominatim: FAIL ({e})")
            else:
                results.append(f"  Nominatim: Not configured")

            # PIL check
            try:
                from PIL import Image
                results.append(f"  PIL: OK")
            except ImportError:
                results.append(f"  PIL: Not installed")

            result_text = "Test Results:\n" + "\n".join(results)
            self.app.call_from_thread(lambda: test_results.update(result_text))
            self.app.call_from_thread(self._restore_config_inputs)

        thread = threading.Thread(target=run_tests, daemon=True)
        thread.start()

    def _restore_config_inputs(self):
        """Restore hidden config inputs after test."""
        try:
            input_widget = self.query_one("#maps-config-value", Input)
            input_widget.display = True
            save_btn = self.query_one("#maps-config-save", Button)
            save_btn.display = True
        except Exception:
            pass

    def add_event(self, event: ServiceEvent):
        """Add event and update stats."""
        super().add_event(event)
        try:
            count_label = self.query_one("#maps-count", Static)
            count_label.update(str(self._request_count))
        except Exception:
            pass
