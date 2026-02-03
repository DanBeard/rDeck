"""Configuration management for companion server."""

import json
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Config:
    """Server configuration."""

    # Data directory for persistence
    data_dir: Path = field(default_factory=lambda: Path.home() / ".companion-server")

    # Server identity name (shown in announces)
    server_name: str = "Companion Server"

    # Services to offer
    enabled_services: list[str] = field(default_factory=lambda: ["ntp", "search"])

    # Search service settings
    search_max_results: int = 5

    # NTP service settings
    ntp_refresh_interval: int = 3600  # seconds

    def __post_init__(self):
        """Ensure data directory exists and load config from disk if present."""
        self.data_dir = Path(self.data_dir)
        self.data_dir.mkdir(parents=True, exist_ok=True)

        # Create subdirectories
        (self.data_dir / "reticulum").mkdir(exist_ok=True)

        # Load config from file if exists
        config_file = self.data_dir / "config.json"
        if config_file.exists():
            self._load_from_file(config_file)
        else:
            self._save_to_file(config_file)

    def _load_from_file(self, path: Path):
        """Load configuration from JSON file."""
        with open(path) as f:
            data = json.load(f)

        self.server_name = data.get("server_name", self.server_name)
        self.enabled_services = data.get("enabled_services", self.enabled_services)
        self.search_max_results = data.get("search_max_results", self.search_max_results)
        self.ntp_refresh_interval = data.get("ntp_refresh_interval", self.ntp_refresh_interval)

    def _save_to_file(self, path: Path):
        """Save configuration to JSON file."""
        data = {
            "server_name": self.server_name,
            "enabled_services": self.enabled_services,
            "search_max_results": self.search_max_results,
            "ntp_refresh_interval": self.ntp_refresh_interval,
        }
        with open(path, "w") as f:
            json.dump(data, f, indent=2)

    @property
    def identity_path(self) -> Path:
        """Path to Reticulum identity file."""
        return self.data_dir / "reticulum" / "identity"

    @property
    def trust_file(self) -> Path:
        """Path to trust state file."""
        return self.data_dir / "trust.json"
