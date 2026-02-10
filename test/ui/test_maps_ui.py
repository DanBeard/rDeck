#!/usr/bin/env python3
"""
UI Tests for Maps App

These tests verify that the Maps app:
1. Launches without crashing
2. Renders meaningful content (tiles, buttons, status bar)
3. Produces reasonably-sized screenshots
4. Has visible zoom button area

Run with:
    cd test/ui && python -m pytest test_maps_ui.py -v
"""

import os
import sys
import pytest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from capture import EmulatorCapture, CaptureConfig

TEST_DIR = Path(__file__).parent
OUTPUT_DIR = TEST_DIR / "output"


@pytest.fixture(scope="module")
def emulator():
    """Shared emulator instance for all Maps tests."""
    config = CaptureConfig(
        display_num=98,  # Different display to avoid conflicts with test_ui.py
        startup_delay=5.0,
    )
    with EmulatorCapture(config) as emu:
        yield emu


@pytest.fixture(scope="module")
def output_dir():
    """Ensure output directory exists."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    return OUTPUT_DIR


class TestMapsLaunch:
    """Test that the Maps app launches and renders."""

    def test_maps_launches_successfully(self, emulator, output_dir):
        """Verify Maps app launches and produces a screenshot."""
        output_path = output_dir / "maps_launch_test.png"
        success = emulator.launch_and_capture("Maps", output_path)

        assert success, "Maps app failed to launch or capture"
        assert output_path.exists(), "Screenshot not created for Maps"
        assert output_path.stat().st_size > 100, "Screenshot too small — likely blank"

    def test_maps_renders_content(self, emulator, output_dir):
        """Verify Maps app renders meaningful content (not just a blank screen)."""
        output_path = output_dir / "maps_content_test.png"
        success = emulator.launch_and_capture("Maps", output_path)

        assert success, "Maps app failed to launch"
        # A rendered map with status bar, buttons, and tile area should be >2KB
        assert output_path.stat().st_size > 2000, (
            f"Screenshot only {output_path.stat().st_size} bytes — "
            "likely missing tiles/buttons/status bar"
        )

    def test_maps_screenshot_reasonable_size(self, emulator, output_dir):
        """Verify screenshot is within expected size range."""
        output_path = output_dir / "maps_size_test.png"
        success = emulator.launch_and_capture("Maps", output_path)

        assert success, "Maps app failed to launch"
        size = output_path.stat().st_size
        assert 1_000 < size < 500_000, (
            f"Screenshot size {size} bytes outside expected range (1KB-500KB)"
        )


class TestMapsZoomButtons:
    """Test that zoom buttons are visible in the Maps UI."""

    def test_maps_has_zoom_buttons(self, emulator, output_dir):
        """Verify the bottom-right region has non-white content (zoom buttons)."""
        output_path = output_dir / "maps_zoom_buttons_test.png"
        success = emulator.launch_and_capture("Maps", output_path)

        assert success, "Maps app failed to launch"

        try:
            from PIL import Image
        except ImportError:
            pytest.skip("PIL not available for pixel-level analysis")

        img = Image.open(output_path)
        w, h = img.size

        # Zoom buttons are in bottom-right area, above the status bar
        # Check a region roughly where buttons should be
        btn_region = img.crop((w - 60, h - 120, w - 2, h - 25))
        pixels = list(btn_region.getdata())

        # Count non-white pixels (buttons have black borders and text)
        non_white = sum(1 for p in pixels if (
            p[0] < 200 or p[1] < 200 or p[2] < 200
        ) if isinstance(p, tuple) and len(p) >= 3)

        # If grayscale
        if pixels and not isinstance(pixels[0], tuple):
            non_white = sum(1 for p in pixels if p < 200)

        total = len(pixels)
        assert non_white > total * 0.02, (
            f"Bottom-right region appears blank ({non_white}/{total} non-white pixels) — "
            "zoom buttons may not be rendering"
        )
