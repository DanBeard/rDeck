# rDeck UI Testing Framework

This framework enables visual UI testing of the rDeck emulator. It allows Claude Code agents and developers to:

- **Capture screenshots** of any app in the emulator
- **Verify apps launch** without crashing
- **Review UI visually** to identify improvements
- **Run visual regression tests** comparing against baselines

## For Claude Code Agents

### Quick Screenshot Capture

To capture a screenshot of any app for review:

```bash
cd /home/user/rDeck

# Start Xvfb virtual display
Xvfb :99 -screen 0 640x480x24 &
sleep 2

# Run emulator with specific app
DISPLAY=:99 .pio/build/emulator_64bits/program --launch-app Settings &
sleep 5

# Capture screenshot
DISPLAY=:99 xwd -root | gm convert xwd:- /path/to/screenshot.png

# View the screenshot
# Use the Read tool on the PNG file - Claude can see images!
```

### Using the Python Framework

```python
from test.ui.capture import EmulatorCapture

with EmulatorCapture() as emu:
    # Capture single app
    emu.launch_and_capture("Settings", "settings.png")

    # Capture multiple apps
    for app in ["Clock", "Notes", "Settings", "Maps"]:
        emu.launch_and_capture(app, f"{app.lower()}.png")
```

### Command Line Usage

```bash
cd test/ui

# Capture single app
python capture.py --app Settings --output settings.png

# Capture all apps
python capture.py --all --output-dir ./screenshots

# Run UI tests
python -m pytest test_ui.py -v

# Capture baselines for regression testing
python test_ui.py --capture-baselines
```

## Available Apps

| App | Description | Status |
|-----|-------------|--------|
| Clock | Time and date display | Stable |
| Notes | Note-taking app | Stable |
| Settings | System settings | Stable |
| Search | Web search via companion server | Stable |
| Maps | Offline maps (planned) | Stable |
| uChat | LXMF messaging | May crash with corrupt data |

## Requirements

- **Xvfb** - Virtual framebuffer for headless display
- **xwd** - X Window Dump utility
- **GraphicsMagick (gm)** - Image conversion
- **Built emulator** at `.pio/build/emulator_64bits/program`

Install on Debian/Ubuntu:
```bash
apt install xvfb x11-apps graphicsmagick
```

## Directory Structure

```
test/ui/
├── README.md           # This file
├── capture.py          # Screenshot capture utilities
├── test_ui.py          # Pytest-based UI tests
├── baseline/           # Baseline screenshots for regression
└── output/             # Test output screenshots
```

## Troubleshooting

### "Emulator crashed" / Segfault

The emulator may crash if there's corrupted data in `emu_data/`. Fix:

```bash
# Clear corrupted identity/LXMF data
rm -rf emu_data/reticulum emu_data/lxmf

# Ensure proper settings for TCP mode (WiFi/TCP required for some tests)
cat > emu_data/settings.json << 'EOF'
{
  "network": {
    "tcp_host": "127.0.0.1",
    "tcp_port": 4242,
    "enabled": true
  }
}
EOF
```

### "Xvfb not found"

Install Xvfb:
```bash
apt install xvfb
```

### Screenshots are blank/black

- Increase `startup_delay` in CaptureConfig (default 4 seconds)
- Check if emulator is actually running
- Verify DISPLAY environment variable matches Xvfb

### "gm convert failed"

Install GraphicsMagick:
```bash
apt install graphicsmagick
```

## UI Review Guidelines

When reviewing UI screenshots, consider:

1. **E-ink Optimization**
   - High contrast (black/white works best)
   - Avoid gradients or complex graphics
   - Minimize screen refreshes

2. **Readability**
   - Font size appropriate for screen (240x320)
   - Text not cut off or truncated
   - Clear visual hierarchy

3. **Layout**
   - Proper spacing and alignment
   - Touch targets appropriately sized
   - Consistent styling across apps

4. **Functionality**
   - All expected elements visible
   - Interactive elements distinguishable
   - Error states clearly communicated

## Example: Visual UI Review Session

```python
#!/usr/bin/env python3
"""Example: Capture all apps for UI review."""

from pathlib import Path
from capture import EmulatorCapture

OUTPUT_DIR = Path("./review_screenshots")
OUTPUT_DIR.mkdir(exist_ok=True)

apps = ["Clock", "Notes", "Settings", "Search", "Maps"]

with EmulatorCapture() as emu:
    for app in apps:
        output = OUTPUT_DIR / f"{app.lower()}.png"
        if emu.launch_and_capture(app, output):
            print(f"Captured {app} -> {output}")
        else:
            print(f"FAILED: {app}")
            print(f"Output: {emu.get_emulator_output()[-500:]}")

print(f"\nScreenshots saved to {OUTPUT_DIR}")
print("Use Claude's Read tool to view the PNG files for review.")
```

## Integration with CI

Add to your CI pipeline:

```yaml
test-ui:
  script:
    - apt-get install -y xvfb graphicsmagick
    - cd test/ui
    - python -m pytest test_ui.py -v --tb=short
```

## Contributing

When modifying UI:

1. Capture baseline screenshots before changes
2. Make UI modifications
3. Capture new screenshots
4. Compare visually or run regression tests
5. Update baselines if changes are intentional
