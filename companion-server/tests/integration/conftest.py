"""Pytest fixtures for E2E integration tests.

These fixtures manage the lifecycle of:
- Companion server process (headless mode)
- rDeck emulator process (with Xvfb if headless)
- Temporary directories for test data
"""

import os
import signal
import subprocess
import tempfile
import time
import pytest
from pathlib import Path
from typing import Generator, Optional


class ProcessManager:
    """Manages a subprocess with cleanup."""

    def __init__(self, name: str):
        self.name = name
        self.process: Optional[subprocess.Popen] = None
        self.output_file: Optional[Path] = None

    def start(self, cmd: list[str], cwd: Optional[Path] = None, env: Optional[dict] = None):
        """Start the process."""
        if self.process:
            raise RuntimeError(f"{self.name} already running")

        # Create output file for capturing logs
        self.output_file = Path(tempfile.mktemp(prefix=f"{self.name}_", suffix=".log"))

        full_env = os.environ.copy()
        if env:
            full_env.update(env)

        with open(self.output_file, "w") as f:
            self.process = subprocess.Popen(
                cmd,
                cwd=cwd,
                env=full_env,
                stdout=f,
                stderr=subprocess.STDOUT,
            )

    def stop(self, timeout: float = 5.0):
        """Stop the process gracefully."""
        if not self.process:
            return

        # Send SIGTERM first
        self.process.terminate()
        try:
            self.process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            # Force kill if still running
            self.process.kill()
            self.process.wait()

        self.process = None

    def is_running(self) -> bool:
        """Check if process is still running."""
        if not self.process:
            return False
        return self.process.poll() is None

    def get_output(self) -> str:
        """Get captured output."""
        if self.output_file and self.output_file.exists():
            return self.output_file.read_text()
        return ""


@pytest.fixture
def temp_data_dir() -> Generator[Path, None, None]:
    """Create a temporary data directory for tests."""
    with tempfile.TemporaryDirectory(prefix="e2e_test_") as tmpdir:
        yield Path(tmpdir)


@pytest.fixture
def companion_server(temp_data_dir: Path) -> Generator[ProcessManager, None, None]:
    """Start companion server in headless mode.

    The server uses a temporary data directory and runs without TUI.
    """
    import sys

    manager = ProcessManager("companion_server")

    # Find companion server directory
    companion_dir = Path(__file__).parent.parent.parent

    # Create server data directory
    server_data = temp_data_dir / "server"
    server_data.mkdir()

    # Use the same Python interpreter that's running the tests (from venv)
    python_exe = sys.executable

    cmd = [
        python_exe, "-m", "companion_server",
        "--headless",
        "--verbose",
        "--data-dir", str(server_data),
    ]

    manager.start(cmd, cwd=companion_dir)

    # Wait for server to initialize
    time.sleep(3)

    if not manager.is_running():
        output = manager.get_output()
        pytest.fail(f"Companion server failed to start:\n{output}")

    yield manager

    manager.stop()


@pytest.fixture
def xvfb_display() -> Generator[Optional[str], None, None]:
    """Start Xvfb virtual display if needed.

    Returns the DISPLAY environment variable to use.
    """
    # Check if we already have a display
    if os.environ.get("DISPLAY"):
        yield os.environ["DISPLAY"]
        return

    # Try to start Xvfb
    try:
        # Find an available display number
        display_num = 99
        display = f":{display_num}"

        process = subprocess.Popen(
            ["Xvfb", display, "-screen", "0", "640x480x24"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        # Wait for Xvfb to start
        time.sleep(1)

        if process.poll() is not None:
            pytest.skip("Xvfb failed to start")
            return

        yield display

        # Cleanup
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()

    except FileNotFoundError:
        pytest.skip("Xvfb not available")


@pytest.fixture
def emulator(temp_data_dir: Path, xvfb_display: Optional[str]) -> Generator[ProcessManager, None, None]:
    """Start rDeck emulator.

    Requires Xvfb on headless systems for SDL2.
    """
    manager = ProcessManager("emulator")

    # Find project root
    project_root = Path(__file__).parent.parent.parent.parent

    # Check if emulator binary exists
    emulator_path = project_root / ".pio" / "build" / "emulator_64bits" / "program"
    if not emulator_path.exists():
        pytest.skip(f"Emulator not built: {emulator_path}")

    # Create emulator config directory
    emulator_config = temp_data_dir / "emulator"
    emulator_config.mkdir()

    env = {
        "RDECK_CONFIG_DIR": str(emulator_config),
    }
    if xvfb_display:
        env["DISPLAY"] = xvfb_display

    cmd = [str(emulator_path)]

    manager.start(cmd, cwd=project_root, env=env)

    # Wait for emulator to initialize
    time.sleep(5)

    if not manager.is_running():
        output = manager.get_output()
        pytest.fail(f"Emulator failed to start:\n{output}")

    yield manager

    manager.stop()


@pytest.fixture
def running_server_and_emulator(
    companion_server: ProcessManager,
    emulator: ProcessManager,
) -> Generator[tuple[ProcessManager, ProcessManager], None, None]:
    """Fixture that provides both running server and emulator.

    Waits for both to be ready before yielding.
    """
    # Additional wait for network discovery
    time.sleep(3)

    yield (companion_server, emulator)


@pytest.fixture
def emulator_with_app_launcher(
    temp_data_dir: Path, xvfb_display: Optional[str]
) -> Generator[callable, None, None]:
    """Factory fixture to launch emulator with a specific app auto-launched.

    Usage:
        def test_my_app(emulator_with_app_launcher):
            manager = emulator_with_app_launcher("Settings")
            # ... test code ...
    """
    managers: list[ProcessManager] = []

    def launcher(app_name: str, max_frames: int = 200) -> ProcessManager:
        manager = ProcessManager(f"emulator_{app_name}")

        # Find project root
        project_root = Path(__file__).parent.parent.parent.parent

        # Check if emulator binary exists
        emulator_path = project_root / ".pio" / "build" / "emulator_64bits" / "program"
        if not emulator_path.exists():
            pytest.skip(f"Emulator not built: {emulator_path}")

        # Create emulator config directory
        emulator_config = temp_data_dir / f"emulator_{app_name}"
        emulator_config.mkdir(exist_ok=True)

        env = {
            "RDECK_CONFIG_DIR": str(emulator_config),
        }
        if xvfb_display:
            env["DISPLAY"] = xvfb_display

        cmd = [
            str(emulator_path),
            "--launch-app", app_name,
            "--max-frames", str(max_frames),
        ]

        manager.start(cmd, cwd=project_root, env=env)

        # Wait for emulator to initialize
        time.sleep(2)

        managers.append(manager)
        return manager

    yield launcher

    # Cleanup all managers
    for manager in managers:
        if manager.is_running():
            manager.stop(timeout=5.0)
