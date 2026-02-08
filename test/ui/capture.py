#!/usr/bin/env python3
"""
UI Screenshot Capture Utility for rDeck Emulator

This module provides utilities for capturing screenshots of the rDeck emulator
running in a virtual display (Xvfb). It can be used for:
- Visual UI testing
- Generating documentation screenshots
- UI review and improvement analysis

Requirements:
- Xvfb (virtual framebuffer)
- xwd (X Window Dump)
- GraphicsMagick (gm) or PIL/Pillow for image conversion
- Built emulator at .pio/build/emulator_64bits/program

Usage:
    from capture import EmulatorCapture

    with EmulatorCapture() as emu:
        screenshot = emu.launch_and_capture("Settings")
        screenshot.save("settings.png")
"""

import os
import subprocess
import tempfile
import time
import signal
from pathlib import Path
from typing import Optional, Union
from dataclasses import dataclass
from contextlib import contextmanager

# Try to import PIL, fall back to GraphicsMagick CLI if not available
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


@dataclass
class CaptureConfig:
    """Configuration for screenshot capture."""
    display_num: int = 99
    screen_width: int = 640
    screen_height: int = 480
    color_depth: int = 24
    startup_delay: float = 4.0  # Time to wait for app to render
    project_root: Optional[Path] = None
    emulator_path: Optional[Path] = None

    def __post_init__(self):
        if self.project_root is None:
            # Try to find project root
            current = Path(__file__).resolve()
            for parent in current.parents:
                if (parent / "platformio.ini").exists():
                    self.project_root = parent
                    break
            else:
                self.project_root = Path.cwd()

        if self.emulator_path is None:
            self.emulator_path = self.project_root / ".pio" / "build" / "emulator_64bits" / "program"


class XvfbManager:
    """Manages Xvfb virtual display lifecycle."""

    def __init__(self, config: CaptureConfig):
        self.config = config
        self.process: Optional[subprocess.Popen] = None
        self.display = f":{config.display_num}"

    def start(self) -> bool:
        """Start Xvfb virtual display."""
        if self.process is not None:
            return True

        cmd = [
            "Xvfb", self.display,
            "-screen", "0",
            f"{self.config.screen_width}x{self.config.screen_height}x{self.config.color_depth}"
        ]

        try:
            self.process = subprocess.Popen(
                cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            time.sleep(1)  # Wait for Xvfb to initialize

            if self.process.poll() is not None:
                self.process = None
                return False

            return True
        except FileNotFoundError:
            print("ERROR: Xvfb not found. Install with: apt install xvfb")
            return False

    def stop(self):
        """Stop Xvfb virtual display."""
        if self.process is not None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
            self.process = None

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()


class EmulatorProcess:
    """Manages emulator process lifecycle."""

    def __init__(self, config: CaptureConfig, display: str):
        self.config = config
        self.display = display
        self.process: Optional[subprocess.Popen] = None
        self.output_file: Optional[Path] = None

    def start(self, app_name: Optional[str] = None) -> bool:
        """Start the emulator, optionally launching a specific app."""
        if not self.config.emulator_path.exists():
            print(f"ERROR: Emulator not built at {self.config.emulator_path}")
            print("Build with: pio run -e emulator_64bits")
            return False

        cmd = [str(self.config.emulator_path)]
        if app_name:
            cmd.extend(["--launch-app", app_name])

        env = os.environ.copy()
        env["DISPLAY"] = self.display

        # Create temp file for output
        self.output_file = Path(tempfile.mktemp(prefix="emu_", suffix=".log"))

        with open(self.output_file, "w") as f:
            self.process = subprocess.Popen(
                cmd,
                cwd=self.config.project_root,
                env=env,
                stdout=f,
                stderr=subprocess.STDOUT,
            )

        # Wait for startup
        time.sleep(self.config.startup_delay)

        if self.process.poll() is not None:
            # Process exited - likely crashed
            return False

        return True

    def stop(self):
        """Stop the emulator."""
        if self.process is not None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
            self.process = None

    def is_running(self) -> bool:
        """Check if emulator is still running."""
        return self.process is not None and self.process.poll() is None

    def get_output(self) -> str:
        """Get captured output from emulator."""
        if self.output_file and self.output_file.exists():
            return self.output_file.read_text()
        return ""


class ScreenshotCapture:
    """Captures screenshots from X display."""

    def __init__(self, display: str):
        self.display = display

    def capture_to_file(self, output_path: Union[str, Path]) -> bool:
        """Capture screenshot and save to file."""
        output_path = Path(output_path)

        env = os.environ.copy()
        env["DISPLAY"] = self.display

        # Use xwd to capture, then convert
        with tempfile.NamedTemporaryFile(suffix=".xwd", delete=False) as tmp:
            tmp_path = tmp.name

        try:
            # Capture with xwd
            result = subprocess.run(
                ["xwd", "-root", "-silent"],
                env=env,
                capture_output=True,
            )

            if result.returncode != 0:
                print(f"xwd failed: {result.stderr.decode()}")
                return False

            # Write xwd data
            with open(tmp_path, "wb") as f:
                f.write(result.stdout)

            # Convert to PNG
            if HAS_PIL:
                # Use PIL if available (not directly - xwd format needs special handling)
                # Fall back to gm
                pass

            # Use GraphicsMagick
            result = subprocess.run(
                ["gm", "convert", tmp_path, str(output_path)],
                capture_output=True,
            )

            if result.returncode != 0:
                print(f"gm convert failed: {result.stderr.decode()}")
                return False

            return output_path.exists()

        finally:
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)

    def capture_to_pil(self) -> Optional["Image.Image"]:
        """Capture screenshot and return as PIL Image."""
        if not HAS_PIL:
            print("PIL not available")
            return None

        with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
            tmp_path = tmp.name

        try:
            if self.capture_to_file(tmp_path):
                return Image.open(tmp_path).copy()
        finally:
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)

        return None


class EmulatorCapture:
    """
    High-level interface for capturing emulator screenshots.

    Usage:
        with EmulatorCapture() as emu:
            # Capture Settings app
            emu.launch_and_capture("Settings", "screenshots/settings.png")

            # Capture Clock app
            emu.launch_and_capture("Clock", "screenshots/clock.png")
    """

    def __init__(self, config: Optional[CaptureConfig] = None):
        self.config = config or CaptureConfig()
        self.xvfb: Optional[XvfbManager] = None
        self.emulator: Optional[EmulatorProcess] = None
        self.capture: Optional[ScreenshotCapture] = None

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()

    def start(self) -> bool:
        """Initialize Xvfb display."""
        self.xvfb = XvfbManager(self.config)
        if not self.xvfb.start():
            return False
        self.capture = ScreenshotCapture(self.xvfb.display)
        return True

    def stop(self):
        """Clean up resources."""
        if self.emulator:
            self.emulator.stop()
            self.emulator = None
        if self.xvfb:
            self.xvfb.stop()
            self.xvfb = None

    def launch_app(self, app_name: str) -> bool:
        """Launch an app in the emulator."""
        # Stop any existing emulator
        if self.emulator:
            self.emulator.stop()

        self.emulator = EmulatorProcess(self.config, self.xvfb.display)
        return self.emulator.start(app_name)

    def capture_screenshot(self, output_path: Union[str, Path]) -> bool:
        """Capture current screen to file."""
        if not self.capture:
            return False
        return self.capture.capture_to_file(output_path)

    def launch_and_capture(
        self,
        app_name: str,
        output_path: Union[str, Path],
        delay: Optional[float] = None
    ) -> bool:
        """
        Launch an app and capture a screenshot.

        Args:
            app_name: Name of app to launch (e.g., "Settings", "Clock", "uChat")
            output_path: Where to save the screenshot
            delay: Additional delay before capture (seconds)

        Returns:
            True if successful, False otherwise
        """
        if not self.launch_app(app_name):
            print(f"Failed to launch {app_name}")
            if self.emulator:
                print(f"Output: {self.emulator.get_output()[-500:]}")
            return False

        if delay:
            time.sleep(delay)

        if not self.capture_screenshot(output_path):
            print(f"Failed to capture screenshot for {app_name}")
            return False

        # Stop emulator after capture
        self.emulator.stop()
        time.sleep(0.5)  # Brief pause between apps

        return True

    def is_emulator_running(self) -> bool:
        """Check if emulator is currently running."""
        return self.emulator is not None and self.emulator.is_running()

    def get_emulator_output(self) -> str:
        """Get emulator console output."""
        if self.emulator:
            return self.emulator.get_output()
        return ""


# Convenience functions for quick capture
def capture_app(app_name: str, output_path: Union[str, Path]) -> bool:
    """Quickly capture a single app screenshot."""
    with EmulatorCapture() as emu:
        return emu.launch_and_capture(app_name, output_path)


def capture_all_apps(output_dir: Union[str, Path]) -> dict:
    """Capture screenshots of all apps."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    apps = ["Clock", "Notes", "Settings", "uChat", "Search", "Maps"]
    results = {}

    with EmulatorCapture() as emu:
        for app in apps:
            output_path = output_dir / f"{app.lower()}.png"
            success = emu.launch_and_capture(app, output_path)
            results[app] = {
                "success": success,
                "path": str(output_path) if success else None
            }
            print(f"{'[OK]' if success else '[FAIL]'} {app}")

    return results


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Capture rDeck emulator screenshots")
    parser.add_argument("--app", help="App to capture (e.g., Settings, Clock)")
    parser.add_argument("--output", "-o", help="Output file path")
    parser.add_argument("--all", action="store_true", help="Capture all apps")
    parser.add_argument("--output-dir", default="./screenshots", help="Output directory for --all")

    args = parser.parse_args()

    if args.all:
        results = capture_all_apps(args.output_dir)
        print(f"\nCaptured {sum(1 for r in results.values() if r['success'])}/{len(results)} apps")
    elif args.app:
        output = args.output or f"{args.app.lower()}.png"
        if capture_app(args.app, output):
            print(f"Saved to {output}")
        else:
            print("Capture failed")
            exit(1)
    else:
        parser.print_help()
