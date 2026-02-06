"""
rDeck UI Testing Framework

This package provides utilities for visual testing of the rDeck emulator.

Usage:
    from test.ui.capture import EmulatorCapture

    with EmulatorCapture() as emu:
        emu.launch_and_capture("Settings", "settings.png")
"""

from .capture import (
    EmulatorCapture,
    CaptureConfig,
    capture_app,
    capture_all_apps,
)

__all__ = [
    "EmulatorCapture",
    "CaptureConfig",
    "capture_app",
    "capture_all_apps",
]
