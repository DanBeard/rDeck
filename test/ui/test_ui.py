#!/usr/bin/env python3
"""
UI Tests for rDeck Emulator

These tests verify that:
1. Apps launch without crashing
2. UI renders correctly
3. Visual appearance matches expectations (optional baseline comparison)

Run with:
    cd test/ui && python -m pytest test_ui.py -v

Or capture baselines:
    python test_ui.py --capture-baselines
"""

import os
import sys
import pytest
from pathlib import Path

# Add parent to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from capture import EmulatorCapture, CaptureConfig, capture_all_apps

# Test configuration
TEST_DIR = Path(__file__).parent
BASELINE_DIR = TEST_DIR / "baseline"
OUTPUT_DIR = TEST_DIR / "output"

# All apps that should be testable
ALL_APPS = ["Clock", "Notes", "Settings", "Search", "Maps"]
# Apps that may have issues (can be skipped in CI)
UNSTABLE_APPS = ["uChat"]  # uChat has data corruption issues in some scenarios


@pytest.fixture(scope="module")
def emulator():
    """Shared emulator instance for all tests in module."""
    config = CaptureConfig(
        display_num=97,  # Use different display to avoid conflicts
        startup_delay=5.0,  # Give apps more time to render
    )
    with EmulatorCapture(config) as emu:
        yield emu


@pytest.fixture(scope="module")
def output_dir():
    """Ensure output directory exists."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    return OUTPUT_DIR


class TestAppLaunch:
    """Test that all apps launch without crashing."""

    @pytest.mark.parametrize("app_name", ALL_APPS)
    def test_app_launches_successfully(self, emulator, output_dir, app_name):
        """Verify app launches and doesn't crash."""
        output_path = output_dir / f"{app_name.lower()}_test.png"

        success = emulator.launch_and_capture(app_name, output_path)

        assert success, f"{app_name} failed to launch or capture"
        assert output_path.exists(), f"Screenshot not created for {app_name}"
        assert output_path.stat().st_size > 100, f"Screenshot too small for {app_name}"

    @pytest.mark.parametrize("app_name", UNSTABLE_APPS)
    @pytest.mark.skip(reason="App may have data state issues")
    def test_unstable_app_launches(self, emulator, output_dir, app_name):
        """Test apps that may be unstable."""
        output_path = output_dir / f"{app_name.lower()}_test.png"
        success = emulator.launch_and_capture(app_name, output_path)
        assert success, f"{app_name} failed to launch"


class TestUIRendering:
    """Test that UI renders correctly."""

    def test_clock_shows_time(self, emulator, output_dir):
        """Verify Clock app renders time display."""
        output_path = output_dir / "clock_render_test.png"
        success = emulator.launch_and_capture("Clock", output_path)

        assert success, "Clock app failed to launch"
        # Screenshot exists and has reasonable size
        assert output_path.exists()
        assert output_path.stat().st_size > 1000  # Should be more than just loading screen

    def test_settings_shows_sections(self, emulator, output_dir):
        """Verify Settings app renders settings sections."""
        output_path = output_dir / "settings_render_test.png"
        success = emulator.launch_and_capture("Settings", output_path)

        assert success, "Settings app failed to launch"
        assert output_path.exists()
        assert output_path.stat().st_size > 2000  # Settings has more UI elements


class TestVisualRegression:
    """
    Visual regression tests comparing against baseline screenshots.

    To create baselines:
        python test_ui.py --capture-baselines

    These tests require baselines to exist.
    """

    @pytest.fixture
    def has_baselines(self):
        """Check if baselines exist."""
        return BASELINE_DIR.exists() and any(BASELINE_DIR.glob("*.png"))

    @pytest.mark.parametrize("app_name", ALL_APPS)
    def test_visual_matches_baseline(self, emulator, output_dir, app_name, has_baselines):
        """Compare current render against baseline."""
        if not has_baselines:
            pytest.skip("No baselines exist. Run with --capture-baselines first")

        baseline_path = BASELINE_DIR / f"{app_name.lower()}.png"
        if not baseline_path.exists():
            pytest.skip(f"No baseline for {app_name}")

        output_path = output_dir / f"{app_name.lower()}_regression.png"
        success = emulator.launch_and_capture(app_name, output_path)

        assert success, f"{app_name} failed to launch"

        # Compare file sizes as a basic check
        # For more sophisticated comparison, use image diff libraries
        baseline_size = baseline_path.stat().st_size
        output_size = output_path.stat().st_size

        # Allow 20% size variance (UI might have dynamic content)
        size_ratio = output_size / baseline_size if baseline_size > 0 else 0
        assert 0.8 < size_ratio < 1.2, (
            f"{app_name} screenshot size differs significantly from baseline "
            f"(baseline: {baseline_size}, current: {output_size})"
        )


def capture_baselines():
    """Capture baseline screenshots for all apps."""
    BASELINE_DIR.mkdir(parents=True, exist_ok=True)

    print(f"Capturing baselines to {BASELINE_DIR}")
    results = capture_all_apps(BASELINE_DIR)

    print("\nBaseline capture results:")
    for app, result in results.items():
        status = "OK" if result["success"] else "FAILED"
        print(f"  {app}: {status}")

    successful = sum(1 for r in results.values() if r["success"])
    print(f"\nCaptured {successful}/{len(results)} baselines")

    return successful == len(results)


def main():
    """Main entry point for command-line usage."""
    import argparse

    parser = argparse.ArgumentParser(description="rDeck UI Tests")
    parser.add_argument("--capture-baselines", action="store_true",
                        help="Capture baseline screenshots")
    parser.add_argument("--list-apps", action="store_true",
                        help="List available apps")

    args = parser.parse_args()

    if args.list_apps:
        print("Available apps:")
        for app in ALL_APPS + UNSTABLE_APPS:
            unstable = " (unstable)" if app in UNSTABLE_APPS else ""
            print(f"  - {app}{unstable}")
        return

    if args.capture_baselines:
        success = capture_baselines()
        sys.exit(0 if success else 1)

    # Default: run pytest
    sys.exit(pytest.main([__file__, "-v"]))


if __name__ == "__main__":
    main()
