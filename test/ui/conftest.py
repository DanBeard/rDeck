"""
Pytest configuration and fixtures for UI tests.
"""

import os
import pytest
from pathlib import Path


def pytest_configure(config):
    """Configure pytest for UI tests."""
    # Ensure we're not running in a headless environment without Xvfb
    if not os.environ.get("DISPLAY") and not _check_xvfb_available():
        print("WARNING: No DISPLAY and Xvfb not available. UI tests may fail.")


def _check_xvfb_available():
    """Check if Xvfb is available."""
    import shutil
    return shutil.which("Xvfb") is not None


@pytest.fixture(scope="session")
def project_root():
    """Get the project root directory."""
    current = Path(__file__).resolve()
    for parent in current.parents:
        if (parent / "platformio.ini").exists():
            return parent
    return Path.cwd()


@pytest.fixture(scope="session")
def emulator_binary(project_root):
    """Get path to emulator binary, skipping if not built."""
    binary = project_root / ".pio" / "build" / "emulator_64bits" / "program"
    if not binary.exists():
        pytest.skip(f"Emulator not built at {binary}")
    return binary


@pytest.fixture(scope="session")
def clean_emu_data(project_root):
    """
    Ensure clean emulator data directory.

    This fixture clears potentially corrupted identity and LXMF data
    that can cause the emulator to crash.
    """
    import shutil

    emu_data = project_root / "emu_data"

    # Directories that may contain corrupted data
    dirs_to_clean = ["reticulum", "lxmf", "cache"]

    for dir_name in dirs_to_clean:
        dir_path = emu_data / dir_name
        if dir_path.exists():
            shutil.rmtree(dir_path)

    # Ensure proper settings for TCP mode
    settings_file = emu_data / "settings.json"
    if not settings_file.exists() or _needs_tcp_settings(settings_file):
        emu_data.mkdir(parents=True, exist_ok=True)
        settings_file.write_text('''{
  "network": {
    "tcp_host": "127.0.0.1",
    "tcp_port": 4242,
    "enabled": true
  }
}
''')

    return emu_data


def _needs_tcp_settings(settings_file):
    """Check if settings file needs TCP configuration."""
    import json
    try:
        data = json.loads(settings_file.read_text())
        network = data.get("network", {})
        return not network.get("enabled", False)
    except (json.JSONDecodeError, IOError):
        return True
