# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

rDeck is a custom operating system (RetOS) for the LilyGo T-Deck Pro hardware—an ESP32-S3 based handheld device with e-paper display, keyboard, LoRa radio, and GPS. It implements the Reticulum Network Stack for off-grid mesh communication via LXMF messaging.

### Vision

rDeck aims to be a **Reticulum-first off-grid smartphone replacement**. The device operates entirely without internet or cellular connectivity, using LoRa mesh networking for communication. The Companion Server system extends its capabilities by providing infrastructure services (NTP time sync, web search, offline maps) to trusted devices over the mesh network, enabling smartphone-like functionality while maintaining complete independence from traditional infrastructure.

### Key Principles

1. **Off-grid first**: All core functionality works without internet
2. **Encrypted by default**: All mesh communication is end-to-end encrypted via Reticulum
3. **Mutual trust**: Services require explicit trust from both device and server
4. **Cross-platform protocol**: Python and C++ implementations use identical msgpack formats

## Build Commands

```bash
# Build for T-Deck Pro hardware (default)
pio run -e T-Deck-Pro

# Build for desktop emulator (SDL2)
pio run -e emulator_64bits

# Run emulator
.pio/build/emulator_64bits/program

# Upload to device
pio run -e T-Deck-Pro --target upload

# Monitor serial output
pio device monitor

# Run C++ unit tests
pio test -e test_native

# Run Python tests (companion server)
cd companion-server && .venv/bin/pytest
```

## Architecture

### Core System (`src/retOS/`)

| File | Purpose |
|------|---------|
| `retOS.h/cpp` | Main OS class, manages app/service lifecycle, event routing, dual-task architecture (UI task + services task) |
| `retUI.h/cpp` | LVGL-based UI framework with top bar (battery, service icons, back button) and app screen area |
| `Events.h` | Event system for inter-component communication (GPS, messages, time changes, settings changes, search results) |
| `RetRunnable.h` | Base interface for tickable components (apps and services) |
| `TimeHelper.h/cpp` | Time management with source priority system (GPS > NTP > Manual) |

### Hardware Abstraction (`src/retHal/`)

| Component | Description |
|-----------|-------------|
| `RetHal.h` | HAL struct containing pointers to all hardware drivers |
| `Screen/` | Display drivers (E-paper for hardware, SDL2 for emulator) |
| `Keyboard/` | Input handling |
| `Battery/` | Power management |
| `GPS/` | Location and time |
| `Lora/` | Radio interface |

Platform-specific implementations selected at compile time via `RET_PLATFORM_TDECKPRO` or `RET_PLATFORM_EMU` defines.

### Board Support (`src/boards/`)

| File | Purpose |
|------|---------|
| `TDeckPro.h/cpp` | T-Deck Pro hardware configuration and initialization |
| `Emulator.h/cpp` | SDL2-based desktop emulator for development |

### Apps (`src/apps/`)

Apps inherit from `BaseApp`. Single active app at a time.

**Lifecycle**: construct → `startApp()` → `tick()` loop → `stop()` → destroy

| App | File | Description |
|-----|------|-------------|
| Launcher | `Launcher.cpp` | App grid home screen |
| UChat | `UChat.cpp` | LXMF encrypted messaging with conversation persistence |
| Clock | `Clock.cpp` | Time display with timezone support |
| Notes | `Notes.cpp` | Local note-taking with persistence |
| Maps | `Maps.cpp` | Offline map tiles with GPS tracking, pan/zoom, geocoding, routing |
| Settings | `Settings.cpp` | System settings, **Trusted Servers management** |
| WebSearch | `WebSearch.cpp` | Web search via companion server |
| CleanScreen | `CleanScreen.cpp` | Screen clearing utility |

**Register new apps** in `rdeck.ino`:
```cpp
AppFactory<MyApp>("Name", &icon)  // in apps list
```

### Services (`src/services/`)

Services inherit from `BaseService`. Run continuously in background. **Start order matters** — WifiService must start before RnsService.

| Service | File | Description |
|---------|------|-------------|
| GPSService | `GPSService.cpp` | GPS data processing, location updates |
| WifiService | `WifiService.cpp` | WiFi connectivity, TCP interface as alternative to LoRa |
| RnsService | `RnsService.cpp` | Reticulum identity, LoRa interface, LXMF messaging, **trust management, service message routing** |
| TimeService | `TimeService.cpp` | Centralized time management with source priority (GPS > NTP > Manual) |

**Register new services** in `rdeck.ino`:
```cpp
ServiceFactory<MyService>()  // in services list
```

### Reticulum Utilities (`src/services/RnsUtils/`)

| File | Purpose |
|------|---------|
| `TrustedServers.h/cpp` | Trust state persistence to `/trusted_servers.json`, manages pending offers and trusted servers |
| `ServiceProtocol.h/cpp` | Message types and payload serialization matching Python companion server |

## Companion Server (`companion-server/`)

Python-based server providing infrastructure services over Reticulum.

### Structure

```
companion-server/
├── companion_server/
│   ├── __main__.py          # Entry point
│   ├── config.py            # Configuration management
│   ├── reticulum_service.py # RNS/LXMF integration
│   ├── trust_manager.py     # Trust state machine + persistence
│   ├── protocol/
│   │   ├── messages.py      # Message type definitions (must match C++)
│   │   └── serialization.py # Msgpack helpers
│   ├── services/
│   │   ├── base_service.py  # Service interface
│   │   ├── ntp_service.py   # Time synchronization
│   │   ├── search_service.py# DuckDuckGo proxy
│   │   └── maps_service.py  # Map tiles, routing, geocoding
│   └── tui/
│       ├── app.py           # Main Textual app
│       ├── announce_view.py # Announce stream widget
│       └── trust_view.py    # Trusted devices widget
├── docker/                  # Docker infrastructure for maps
│   ├── setup.sh             # Region selection, PBF download, tile generation
│   ├── run.sh               # Start Docker services + companion server TUI
│   ├── docker-compose.yml   # tileserver-gl, Valhalla, Nominatim
│   ├── tileserver-config.json
│   ├── tileserver-style.json # Grayscale style for e-ink
│   ├── reticulum-config     # TCP server interface config
│   └── README.md            # Docker setup documentation
└── tests/                   # Comprehensive test suite
```

### Running

**With maps (Docker):**
```bash
cd companion-server/docker
./setup.sh    # One-time: pick region, download data, generate tiles
./run.sh      # Start tileserver + Valhalla + Nominatim + companion server TUI
```

**Without maps:**
```bash
cd companion-server
python3 -m venv .venv
source .venv/bin/activate
pip install -e .
python -m companion_server
```

## Service Protocol

Messages use LXMF fields with msgpack encoding. **Both Python and C++ must use identical formats.**

### Message Types (defined in both implementations)

```cpp
// C++: src/services/RnsUtils/ServiceProtocol.h
enum class MessageType : uint8_t {
    TRUST_OFFER         = 0x01,  // Server offers services
    TRUST_ACCEPT        = 0x02,  // Device accepts offer
    TRUST_REVOKE        = 0x03,  // Either side revokes
    NTP_REQUEST         = 0x10,  // Device requests time
    NTP_RESPONSE        = 0x11,  // Server responds with time
    SEARCH_REQUEST      = 0x20,  // Device sends query
    SEARCH_RESPONSE     = 0x21,  // Server returns results
    MAP_TILE_REQUEST    = 0x30,  // Device requests map tile
    MAP_TILE_RESPONSE   = 0x31,  // Server returns tile data
    MAP_ROUTE_REQUEST   = 0x33,  // Device requests route
    MAP_ROUTE_RESPONSE  = 0x34,  // Server returns route
    MAP_GEOCODE_REQUEST = 0x35,  // Device searches address
    MAP_GEOCODE_RESPONSE= 0x36,  // Server returns locations
};
```

```python
# Python: companion_server/protocol/messages.py
class MessageType(IntEnum):
    TRUST_OFFER         = 0x01
    TRUST_ACCEPT        = 0x02
    TRUST_REVOKE        = 0x03
    NTP_REQUEST         = 0x10
    NTP_RESPONSE        = 0x11
    SEARCH_REQUEST      = 0x20
    SEARCH_RESPONSE     = 0x21
    MAP_TILE_REQUEST    = 0x30
    MAP_TILE_RESPONSE   = 0x31
    MAP_ROUTE_REQUEST   = 0x33
    MAP_ROUTE_RESPONSE  = 0x34
    MAP_GEOCODE_REQUEST = 0x35
    MAP_GEOCODE_RESPONSE= 0x36
```

### LXMF Fields Structure

```python
fields = {
    "msg_type": <uint8>,      # MessageType enum value
    "service": <string>,      # "trust", "ntp", "search", "maps"
    "payload": <bytes>,       # Msgpack-encoded payload
    "request_id": <uint32>    # For request/response correlation
}
```

### Payload Field Names (must match exactly)

| Message | Fields |
|---------|--------|
| TRUST_OFFER | `server_name`, `services` (array) |
| TRUST_ACCEPT | `device_name` |
| NTP_REQUEST | `client_timestamp` |
| NTP_RESPONSE | `server_timestamp`, `client_timestamp` |
| SEARCH_REQUEST | `query`, `max_results` |
| SEARCH_RESPONSE | `query`, `results` (array of {title, url, snippet}), `error` |
| MAP_TILE_REQUEST | `z`, `x`, `y`, `format` (TileFormat enum) |
| MAP_TILE_RESPONSE | `z`, `x`, `y`, `format`, `chunk_index`, `total_chunks`, `data`, `error` |
| MAP_ROUTE_REQUEST | `start_lat`, `start_lon`, `end_lat`, `end_lon` (int32 * 1e7), `mode` |
| MAP_ROUTE_RESPONSE | `points` (lat/lon pairs * 1e7), `instructions`, `total_distance_m`, `total_time_s`, `error` |
| MAP_GEOCODE_REQUEST | `query`, `bias_lat`, `bias_lon` (int32 * 1e7), `has_bias`, `max_results` |
| MAP_GEOCODE_RESPONSE | `query`, `results` (array of {display_name, lat, lon, type}), `error` |

## Trust Workflow

```
1. rDeck announces on network (existing behavior)
2. Server TUI shows announce in stream
3. Server user clicks "Trust" on rDeck's announce
4. Server sends TRUST_OFFER (name, services list)
5. rDeck receives, stores as "pending" in /trusted_servers.json
6. rDeck user opens Settings → Trusted Servers → sees pending offer
7. rDeck user clicks "Accept"
8. rDeck sends TRUST_ACCEPT message
9. Server receives, marks mutual trust
10. Services now available!
```

## Time Priority System

```cpp
// src/retOS/retosUtils/TimeHelper.h
enum class TimeSource : uint8_t {
    NONE           = 0,  // No time set
    MANUAL         = 1,  // User-set time (lowest priority)
    RETICULUM_NTP  = 2,  // Time from companion server
    GPS            = 3   // GPS time (highest priority)
};
```

Time updates only accepted from equal or higher priority sources. GPS always wins.

## Key Patterns

### Factory Registration

```cpp
// Apps in rdeck.ino
AppFactory<MyApp>("Name", &icon)

// Services in rdeck.ino
ServiceFactory<MyService>()
```

### Fetching Services

```cpp
RnsService* rns = retos->fetchService<RnsService>();
```

### Event Publishing

```cpp
// From a service
publishEvent(Event{
    .type = EventType::TIME_CHANGED,
    .data = {.timeChanged = {.source = TimeSource::GPS}}
});
```

### Sending Service Messages

```cpp
// In RnsService
ServiceMessage msg;
msg.msg_type = MessageType::NTP_REQUEST;
msg.service = "ntp";
msg.request_id = generateRequestId();
// ... set payload
sendServiceMessage(serverHash, msg);
```

### UI Drawing

Apps draw to `screen` (lv_obj_t*). Use `_retos->ui()` for theme colors:
```cpp
lv_obj_set_style_bg_color(obj, _retos->ui()->theme.bg, LV_PART_MAIN);
```

## Testing

### C++ Tests (`test/unit/`)

| Test Suite | Coverage |
|------------|----------|
| `test_service_protocol` | Message types, payload serialization, cross-compat encoding |
| `test_trusted_servers` | Trust state management, persistence, edge cases |
| `test_bytes` | Bytes utility class |
| `test_lxmf` | LXMF message handling |
| `test_crypto` | Cryptographic operations |

Run: `pio test -e test_native`

### Python Tests (`companion-server/tests/`)

| Test File | Coverage |
|-----------|----------|
| `test_protocol.py` | Message types, payload serialization, canonical encoding |
| `test_trust_manager.py` | Trust state transitions, persistence |
| `test_services.py` | NTP and Search service logic |
| `test_cross_compatibility.py` | Verify Python can decode C++ format and vice versa |

Run: `cd companion-server && .venv/bin/pytest`

### UI Tests (`test/ui/`)

Visual UI testing framework for capturing and analyzing emulator screenshots.

**Claude agents can see screenshots!** Use this to review UI, debug visual issues, and verify changes.

| File | Purpose |
|------|---------|
| `capture.py` | Screenshot capture utilities (Xvfb + xwd + GraphicsMagick) |
| `test_ui.py` | Pytest-based UI tests |
| `baseline/` | Baseline screenshots for regression testing |

**Quick Screenshot Capture:**
```bash
# Start Xvfb virtual display
Xvfb :99 -screen 0 640x480x24 &
sleep 2

# Run emulator with specific app
DISPLAY=:99 .pio/build/emulator_64bits/program --launch-app Settings &
sleep 5

# Capture screenshot
DISPLAY=:99 xwd -root | gm convert xwd:- screenshot.png

# View with Claude's Read tool - it can see images!
```

**Python API:**
```python
from test.ui.capture import EmulatorCapture

with EmulatorCapture() as emu:
    emu.launch_and_capture("Settings", "settings.png")
```

**Run UI tests:** `cd test/ui && python -m pytest test_ui.py -v`

**Capture baselines:** `cd test/ui && python test_ui.py --capture-baselines`

## Dependencies

### C++ (PlatformIO)
- LVGL 8.3.x - UI framework
- microReticulum - Reticulum Network Stack (from `DanBeard/microReticulum#aes256-clean`)
- ArduinoJson - JSON/MsgPack serialization
- RadioLib - LoRa radio
- Local libraries in `lib/` (GxEPD2, TinyGPSPlus, XPowersLib, etc.)

### Python (Companion Server)
- rns - Reticulum Network Stack
- lxmf - LXMF messaging
- textual - Terminal UI
- msgpack - Binary serialization
- httpx - HTTP client for search and maps service proxying
- Pillow - Image processing (tile dithering for e-ink)

## Docker Maps Infrastructure (`companion-server/docker/`)

Self-hosted offline maps using OpenStreetMap data. Three Docker services provide tile rendering, routing, and geocoding.

| Service | Port | Purpose |
|---------|------|---------|
| tileserver-gl | 8081 | Renders vector MBTiles to grayscale PNG tiles |
| Valhalla | 8002 | Turn-by-turn routing with elevation data |
| Nominatim | 8080 | Address/place search (geocoding) |

**Setup flow**: `setup.sh` downloads a regional PBF from Geofabrik, generates vector MBTiles via Planetiler, and configures services. `run.sh` starts Docker containers and launches the companion server TUI.

**Tile pipeline**: tileserver-gl renders vector tiles → companion server fetches PNG → resizes to 128x128 → Floyd-Steinberg dithers to 1-bit → optional RLE compression → chunks into ≤200 byte packets for LoRa.

**Coordinates**: All lat/lon values use int32 * 1e7 encoding to avoid floating-point on embedded targets.

See `companion-server/docker/README.md` for region selection, resource requirements, and data management.

## Common Tasks

### Adding a New App

1. Create `src/apps/MyApp.h` and `src/apps/MyApp.cpp`
2. Inherit from `BaseApp`
3. Implement `startApp()`, `tick()`, `stop()`
4. Add icon to `src/apps/icons/`
5. Register in `rdeck.ino`: `AppFactory<MyApp>("MyApp", &myapp_icon)`

### Adding a New Service Message Type

1. Add enum value to both:
   - `src/services/RnsUtils/ServiceProtocol.h` (C++)
   - `companion-server/companion_server/protocol/messages.py` (Python)
2. Create payload struct/dataclass in both
3. Add serialization in both implementations
4. Add handler in `RnsService::handleServiceMessage()` (C++)
5. Add handler in companion server's `ReticulumService` (Python)
6. Add tests in both test suites to verify compatibility

### Adding a New Companion Server Service

1. Create `companion-server/companion_server/services/my_service.py`
2. Inherit from `BaseService`
3. Implement `name` property and `handle_request()` method
4. Register in `ReticulumService.__init__()`
5. Add message types for request/response
6. Add corresponding client code in `RnsService` on rDeck
