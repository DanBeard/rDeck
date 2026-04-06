# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

rDeck is a custom operating system (RetOS) for the LilyGo T-Deck Pro hardware—an ESP32-S3 based handheld device with e-paper display, keyboard, LoRa radio, and GPS. It implements the Reticulum Network Stack for off-grid mesh communication via LXMF messaging.

### Vision

rDeck aims to be a **Reticulum-first off-grid smartphone replacement**. The device operates entirely without internet or cellular connectivity, using LoRa mesh networking for communication. The Companion Server system extends its capabilities by providing infrastructure services (NTP time sync, web search, offline maps, LXMF propagation) to trusted devices over the mesh network, enabling smartphone-like functionality while maintaining complete independence from traditional infrastructure.

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

# Run emulator with a specific app
.pio/build/emulator_64bits/program --launch-app Maps

# Upload to device
pio run -e T-Deck-Pro --target upload

# Monitor serial output
pio device monitor

# Run C++ unit tests
pio test -e test_native

# Run a single C++ test suite
pio test -e test_native -f "unit/test_device_name"

# Run Python tests (companion server)
cd companion-server && .venv/bin/pytest
```

## Architecture

### Core System (`src/retOS/`)

| File | Purpose |
|------|---------|
| `retOS.h/cpp` | Main OS class, manages app/service lifecycle, event routing, dual-task architecture (UI task + services task) |
| `retUI.h/cpp` | LVGL-based UI framework with top bar (battery, service icons, back button) and app screen area |
| `Events.h` | Event system for inter-component communication (GPS, messages, time, settings, search, map tiles, routes, geocode) |
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
| UChat | `UChat.cpp` | LXMF encrypted messaging with conversation persistence, pagination |
| Clock | `Clock.cpp` | Time display with timezone support |
| Notes | `Notes.cpp` | Local note-taking with persistence |
| Maps | `Maps.cpp` | Offline map tiles with GPS tracking, pan/zoom, geocoding, routing/directions, compass bearing |
| Settings | `Settings.cpp` | Date/time, **Trusted Servers management**, device name, identity regeneration |
| WebSearch | `WebSearch.cpp` | Web search via companion server with optional AI summary |
| CleanScreen | `CleanScreen.cpp` | Screen clearing utility |

**Register new apps** in `rdeck.ino`:
```cpp
AppFactory<MyApp>("Name", &icon)  // in apps list
```

### Services (`src/services/`)

Services inherit from `BaseService`. Run continuously in background. **Start order matters** — see `rdeck.ino` for registration order.

| Service | File | Description |
|---------|------|-------------|
| PositionService | `PositionService.cpp` | GPS data processing, location/heading, time sync from GPS |
| WifiService | `WifiService.cpp` | WiFi connectivity, TCP interface as alternative to LoRa |
| RnsService | `RnsService.cpp` | Reticulum identity, LoRa/TCP interfaces, LXMF messaging, trust management, service message routing, propagation sync, device name/userInfo |
| TimeService | `TimeService.cpp` | Centralized time management with source priority (GPS > NTP > Manual), propagation sync scheduling |

**Register new services** in `rdeck.ino`:
```cpp
ServiceFactory<MyService>()  // in services list
```

### Reticulum Utilities (`src/services/RnsUtils/`)

| File | Purpose |
|------|---------|
| `TrustedServers.h/cpp` | Trust state persistence to `/trusted_servers.json`, manages pending offers and trusted servers |
| `ServiceProtocol.h/cpp` | Message types, payload structs, and serialization matching Python companion server |

## Companion Server (`companion-server/`)

Python-based server providing infrastructure services over Reticulum.

### Structure

```
companion-server/
├── companion_server/
│   ├── __main__.py          # Entry point, CLI argument parsing
│   ├── config.py            # Configuration management (propagation, AI summary, maps)
│   ├── reticulum_service.py # RNS/LXMF integration
│   ├── trust_manager.py     # Trust state machine + persistence
│   ├── protocol/
│   │   ├── messages.py      # Message type definitions (must match C++)
│   │   └── serialization.py # Msgpack helpers
│   ├── services/
│   │   ├── base_service.py  # Service interface
│   │   ├── ntp_service.py   # Time synchronization
│   │   ├── search_service.py# DuckDuckGo proxy + optional AI summary (local LLM)
│   │   ├── maps_service.py  # Map tiles, routing, geocoding
│   │   └── propagation_service.py # LXMF store-and-forward via propagation node
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

Both modes use `~/.companion-server/` for identity, trust, and config by default. Override with `--data-dir` or `COMPANION_DATA_DIR` env var.

**With maps (Docker):**
```bash
cd companion-server/docker
./setup.sh              # One-time: pick region, download data, generate tiles
./run.sh                # Start Docker services + companion server TUI with maps
./run.sh --headless -v  # Headless mode (no TUI) with verbose logging
```

**Without maps:**
```bash
cd companion-server
python3 -m venv .venv
source .venv/bin/activate
pip install -e .
python -m companion_server                  # TUI mode, no maps
python -m companion_server --headless       # Headless mode
python -m companion_server --with-maps      # Enable maps with default Docker URLs
python -m companion_server --tcp-port 4242  # Generate Reticulum TCP config
python -m companion_server --reset          # Delete and reset all data
python -m companion_server -v               # Verbose/debug logging
```

## Service Protocol

Messages use LXMF fields with msgpack encoding. **Both Python and C++ must use identical formats.**

### Message Types (defined in both implementations)

```cpp
// C++: src/services/RnsUtils/ServiceProtocol.h
enum class MessageType : uint8_t {
    // Trust management
    TRUST_OFFER         = 0x01,  // Server offers services
    TRUST_ACCEPT        = 0x02,  // Device accepts offer
    TRUST_REVOKE        = 0x03,  // Either side revokes

    // NTP service
    NTP_REQUEST         = 0x10,  // Device requests time
    NTP_RESPONSE        = 0x11,  // Server responds with time

    // Search service
    SEARCH_REQUEST      = 0x20,  // Device sends query
    SEARCH_RESPONSE     = 0x21,  // Server returns results

    // Maps service
    MAP_TILE_REQUEST    = 0x30,  // Device requests map tile
    MAP_TILE_RESPONSE   = 0x31,  // Server returns tile data
    MAP_ROUTE_REQUEST   = 0x33,  // Device requests route
    MAP_ROUTE_RESPONSE  = 0x34,  // Server returns route
    MAP_GEOCODE_REQUEST = 0x35,  // Device searches address
    MAP_GEOCODE_RESPONSE= 0x36,  // Server returns locations

    // Propagation service
    PROP_SYNC_REQUEST   = 0x40,  // Device polls for stored messages
    PROP_SYNC_RESPONSE  = 0x41,  // Server responds with message count
    PROP_MSG_DELIVER    = 0x42,  // Server delivers raw LXMF bytes
    PROP_SUBMIT_REQUEST = 0x43,  // Device submits raw LXMF for propagation
    PROP_SUBMIT_RESPONSE= 0x44,  // Server confirms acceptance
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
    PROP_SYNC_REQUEST   = 0x40
    PROP_SYNC_RESPONSE  = 0x41
    PROP_MSG_DELIVER    = 0x42
    PROP_SUBMIT_REQUEST = 0x43
    PROP_SUBMIT_RESPONSE= 0x44
```

### LXMF Fields Structure

```python
fields = {
    "msg_type": <uint8>,      # MessageType enum value
    "service": <string>,      # "trust", "ntp", "search", "maps", "propagation"
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
| SEARCH_REQUEST | `query`, `max_results`, `ai_summary` (bool) |
| SEARCH_RESPONSE | `query`, `results` (array of {title, url, snippet}), `error`, `summary` (optional AI summary) |
| MAP_TILE_REQUEST | `z`, `x`, `y`, `format` (TileFormat enum) |
| MAP_TILE_RESPONSE | `z`, `x`, `y`, `format`, `chunk_index`, `total_chunks`, `data`, `error` |
| MAP_ROUTE_REQUEST | `start_lat`, `start_lon`, `end_lat`, `end_lon` (int32 * 1e7), `mode` |
| MAP_ROUTE_RESPONSE | `points` (lat/lon pairs * 1e7), `instructions` (array of {distance_m, maneuver, street, bearing}), `total_distance_m`, `total_time_s`, `error` |
| MAP_GEOCODE_REQUEST | `query`, `bias_lat`, `bias_lon` (int32 * 1e7), `has_bias`, `max_results` |
| MAP_GEOCODE_RESPONSE | `query`, `results` (array of {display_name, lat, lon, type}), `error` |
| PROP_SYNC_REQUEST | `lxmf_dest_hash` (16 bytes), `known_ids` (array of transient IDs), `max_messages` |
| PROP_SYNC_RESPONSE | `count`, `error` |
| PROP_MSG_DELIVER | `transient_id` (SHA-256 hash), `raw_lxmf` (raw LXMF packed bytes) |
| PROP_SUBMIT_REQUEST | `raw_lxmf` |
| PROP_SUBMIT_RESPONSE | `accepted`, `transient_id`, `error` |

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
PositionService* pos = retos->fetchService<PositionService>();
```

### Event Publishing

```cpp
// From a service
publishEvent(Event{
    .type = EventType::TIME_CHANGE,
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

### Thread-Safe Action Queue

Apps run on the UI task and services run on the services task. To perform RNS operations from the UI thread (e.g., sending a message when a button is pressed), queue an action:

```cpp
RnsService* rns = retOsGlobalPtr->fetchService<RnsService>();
rns->queueAction([rns]() {
    rns->announce();
});
```

Actions are drained in `RnsService::tick()`, rate-limited to `MAX_ACTIONS_PER_TICK` (3) per tick to avoid overwhelming Transport with rapid-fire packet sends.

### UI Drawing

Apps draw to `screen` (lv_obj_t*). Use `_retos->ui()` for theme colors:
```cpp
lv_obj_set_style_bg_color(obj, _retos->ui()->bg_color(), LV_PART_MAIN);
lv_obj_set_style_text_color(label, _retos->ui()->fg_color(), LV_PART_MAIN);
```

## Coding Standards

### ArduinoJson String Storage

**Never assign raw `const char*` pointers to ArduinoJson documents.** ArduinoJson stores `const char*` as a pointer, not a copy. If the source buffer is freed or overwritten, the JSON value becomes garbage.

```cpp
// BAD — dangling pointer if source is freed:
const char* text = lv_textarea_get_text(ta);
doc["name"] = text;

// GOOD — String is copied by ArduinoJson:
String text = lv_textarea_get_text(ta);
doc["name"] = text;
```

This applies to any temporary `const char*`: LVGL text getters, `.c_str()` on temporaries, stack buffers, etc.

### Temporary Value Lifetime

Never store `const char*` from a temporary. The pointer is invalidated when the temporary is destroyed:

```cpp
// BAD — pointer dangles immediately:
const char* hex = bytes.toHex().c_str();

// GOOD — keep the string alive:
std::string hex = bytes.toHex();
```

### LVGL Callbacks

Static `FunctorCallback` objects must be used for LVGL event callbacks since they must outlive the draw call. Always declare as `static`:

```cpp
static FunctorCallback my_callback;
my_callback = [](lv_event_t* e) { /* ... */ };
lv_obj_add_event_cb(obj, functor_callback, LV_EVENT_CLICKED, &my_callback);
```

### Null Safety

Always check for null before dereferencing ArduinoJson values, especially from settings or config files:

```cpp
// BAD — crashes if key missing:
const char* val = doc["key"].as<const char*>();
strlen(val);  // nullptr dereference

// GOOD:
if (doc.containsKey("key") && !doc["key"].isNull()) {
    const char* val = doc["key"].as<const char*>();
}
```

### Cross-Thread Safety

- UI thread accesses: LVGL objects, app state
- Services thread accesses: RNS, LXMF, network interfaces
- Use `queueAction()` to safely cross from UI → services thread
- Use `publishEvent()` + event handling for services → UI communication
- Protect shared data with `PlatformMutexGuard`

## Testing

### C++ Tests (`test/unit/`)

| Test Suite | Coverage |
|------------|----------|
| `test_service_protocol` | Message types, payload serialization, cross-compat encoding |
| `test_protocol_vectors` | Canonical encoding vectors for cross-language compatibility |
| `test_trusted_servers` | Trust state management, persistence, edge cases |
| `test_device_name` | Device name storage, ArduinoJson String safety, announce format |
| `test_lxmf` | LXMF message handling |
| `test_lxmf_workflow` | Full LXMF message send/receive roundtrip |
| `test_conversation_persistence` | LXMF conversation storage and retrieval |
| `test_announce_format` | Announce msgpack binary format |
| `test_announce_dedup` | Announce deduplication logic |
| `test_action_queue` | Thread-safe action queue |
| `test_maps` | Map tile handling, RLE decompression |
| `test_identity` | Identity key management |
| `test_crypto` | Cryptographic operations |
| `test_bytes` | Bytes utility class |
| `test_packet` | Packet construction and serialization |
| `test_resource` | Resource transfer and bz2 decompression |

Integration tests: `test/integration/test_service_integration`, `test/integration/test_settings`

Run: `pio test -e test_native`

### Python Tests (`companion-server/tests/`)

| Test File | Coverage |
|-----------|----------|
| `test_protocol.py` | Message types, payload serialization, canonical encoding |
| `test_protocol_vectors.py` | Canonical encoding vectors for cross-language compatibility |
| `test_trust_manager.py` | Trust state transitions, persistence |
| `test_services.py` | NTP and Search service logic |
| `test_maps_service.py` | Maps service routing/geocoding |
| `test_propagation_service.py` | Propagation node store-and-forward |
| `test_cross_compatibility.py` | Verify Python can decode C++ format and vice versa |
| `test_reticulum_service.py` | RNS/LXMF integration |
| `test_edge_cases.py` | Edge cases and error handling |
| `test_tui.py` | Terminal UI tests |
| `integration/test_e2e.py` | End-to-end integration tests |

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

## Lessons Learned

Common bugs and pitfalls encountered in this codebase. **Read this before writing new code.**

### 1. ArduinoJson Does NOT Copy `const char*`

When you assign a raw `const char*` to a `JsonDocument`, ArduinoJson stores the **pointer**, not a copy of the string. If the source buffer is later freed, overwritten, or goes out of scope, the JSON value silently becomes garbage.

**This has caused multiple bugs:**
- **Device name corruption** (`Settings.cpp`): `lv_textarea_get_text()` returns a pointer into LVGL's internal buffer. After leaving the Settings screen, the textarea is destroyed and the pointer dangles. The saved name becomes unprintable characters.
- **Identity corruption** (`RnsService.cpp`): `bytes.toHex().c_str()` returns a pointer to a temporary `std::string`. The string is destroyed at the end of the expression, leaving a dangling pointer.

**The fix is always the same:** Copy into `String` (Arduino) or `std::string` (C++) before assigning:
```cpp
String name = lv_textarea_get_text(ta);  // copy first
doc["name"] = name;                       // ArduinoJson copies String contents
```

### 2. Dangling Pointers from Temporaries

Any time you call `.c_str()` on a temporary, the pointer is immediately invalid:
```cpp
const char* bad = someObject.toString().c_str();  // DANGLING
std::string good = someObject.toString();          // SAFE
```

### 3. Thread Safety Between UI and Services

The UI task and services task run on separate threads (on ESP32, separate FreeRTOS tasks). Never call RNS/LXMF functions directly from UI code. Use `rns->queueAction(...)` to safely dispatch work to the services thread. Use events (`publishEvent`) for the reverse direction.

### 4. Null Checks on ArduinoJson Values

`JsonDocument["key"].as<const char*>()` returns `nullptr` if the key doesn't exist. Always check before using:
```cpp
if (doc.containsKey("key")) { ... }
```
This caused a crash in `Settings::stop()` when accessing a timezone value that hadn't been set.

### 5. Protocol Changes Must Be Synchronized

Any change to message types, payload fields, or serialization format must be made in **both** C++ (`ServiceProtocol.h/cpp`) and Python (`messages.py`, `serialization.py`). Always add tests in both test suites. Use the `protocol-sync` skill to verify.
