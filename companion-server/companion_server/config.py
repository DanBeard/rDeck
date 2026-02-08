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

    # AI Summary settings (optional, requires llama-cpp-python)
    ai_summary_enabled: bool = False
    ai_summary_model_path: Optional[str] = None  # Path to GGUF model file
    ai_summary_max_tokens: int = 256
    ai_summary_context_size: int = 2048

    # NTP service settings
    ntp_refresh_interval: int = 3600  # seconds

    # Maps service settings
    maps_enabled: bool = False
    maps_mbtiles_path: Optional[str] = None  # Path to MBTiles file
    maps_valhalla_url: Optional[str] = None  # URL to Valhalla routing server
    maps_nominatim_url: Optional[str] = None  # URL to Nominatim geocoding server
    maps_tileserver_url: Optional[str] = None  # URL to tileserver-gl for vector tile rendering

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
        self.ai_summary_enabled = data.get("ai_summary_enabled", self.ai_summary_enabled)
        self.ai_summary_model_path = data.get("ai_summary_model_path", self.ai_summary_model_path)
        self.ai_summary_max_tokens = data.get("ai_summary_max_tokens", self.ai_summary_max_tokens)
        self.ai_summary_context_size = data.get("ai_summary_context_size", self.ai_summary_context_size)
        self.ntp_refresh_interval = data.get("ntp_refresh_interval", self.ntp_refresh_interval)
        self.maps_enabled = data.get("maps_enabled", self.maps_enabled)
        self.maps_mbtiles_path = data.get("maps_mbtiles_path", self.maps_mbtiles_path)
        self.maps_valhalla_url = data.get("maps_valhalla_url", self.maps_valhalla_url)
        self.maps_nominatim_url = data.get("maps_nominatim_url", self.maps_nominatim_url)
        self.maps_tileserver_url = data.get("maps_tileserver_url", self.maps_tileserver_url)

    def _save_to_file(self, path: Path):
        """Save configuration to JSON file."""
        data = {
            "server_name": self.server_name,
            "enabled_services": self.enabled_services,
            "search_max_results": self.search_max_results,
            "ai_summary_enabled": self.ai_summary_enabled,
            "ai_summary_model_path": self.ai_summary_model_path,
            "ai_summary_max_tokens": self.ai_summary_max_tokens,
            "ai_summary_context_size": self.ai_summary_context_size,
            "ntp_refresh_interval": self.ntp_refresh_interval,
            "maps_enabled": self.maps_enabled,
            "maps_mbtiles_path": self.maps_mbtiles_path,
            "maps_valhalla_url": self.maps_valhalla_url,
            "maps_nominatim_url": self.maps_nominatim_url,
            "maps_tileserver_url": self.maps_tileserver_url,
        }
        with open(path, "w") as f:
            json.dump(data, f, indent=2)

    def save(self):
        """Save current configuration to disk."""
        config_file = self.data_dir / "config.json"
        self._save_to_file(config_file)

    @property
    def identity_path(self) -> Path:
        """Path to Reticulum identity file."""
        return self.data_dir / "reticulum" / "identity"

    @property
    def trust_file(self) -> Path:
        """Path to trust state file."""
        return self.data_dir / "trust.json"
