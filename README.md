# rDeck

A Reticulum-first off-grid smartphone replacement built on the LilyGo T-Deck Pro.

## Vision

rDeck transforms the T-Deck Pro into a fully functional off-grid communication device. Using the Reticulum Network Stack, it provides encrypted mesh messaging without any internet or cellular infrastructure. When paired with a Companion Server, rDeck gains smartphone-like capabilities—time synchronization, web search, offline maps with routing and geocoding—all delivered securely over the mesh network.

**No cell towers. No internet on the device. Just encrypted mesh.**

## Features

### Core Functionality
- **Off-grid Messaging (UChat)**: End-to-end encrypted LXMF messaging over LoRa
- **Address Book**: Store and manage contacts with Reticulum addresses
- **Notes**: Local note-taking with persistence
- **Clock**: Multiple timezone support with automatic time sync

### Companion Server Integration
When connected to a trusted Companion Server:
- **NTP Time Sync**: Accurate time over the mesh network
- **Web Search**: Search the web via DuckDuckGo proxy
- **Offline Maps**: Pan/zoom map tiles, GPS tracking, address search, and routing — all from self-hosted OpenStreetMap data
- **Extensible**: Protocol supports adding new services

### Hardware Features
- **E-paper Display**: Low power, excellent daylight readability
- **GPS**: Location tracking and time synchronization
- **LoRa Radio**: Long-range mesh networking (SX1262)
- **QWERTY Keyboard**: Full text input
- **Battery Powered**: Portable operation

### Development
- **Desktop Emulator**: Full SDL2-based emulator for development without hardware
- **Unit Tests**: Comprehensive test suites for protocol compatibility

## Hardware Requirements

- **LilyGo T-Deck Pro** (primary target)
  - ESP32-S3 processor
  - 2.13" E-paper display
  - SX1262 LoRa radio
  - GPS module
  - QWERTY keyboard
  - Battery management

## Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE plugin)
- For emulator: SDL2 development libraries
  ```bash
  # Debian/Ubuntu
  sudo apt install libsdl2-dev

  # macOS
  brew install sdl2
  ```

### Building for Hardware

```bash
# Build
pio run -e T-Deck-Pro

# Upload to device
pio run -e T-Deck-Pro --target upload

# Monitor serial output
pio device monitor
```

### Building the Desktop Emulator

```bash
# Build
pio run -e emulator_64bits

# Run
.pio/build/emulator_64bits/program
```

### Running the Companion Server

The Companion Server provides services to rDeck devices over Reticulum. The Docker setup includes map tile rendering, routing, and geocoding.

```bash
cd companion-server/docker

# One-time setup: pick a region, download OSM data, generate tiles
./setup.sh

# Start Docker services (tileserver, Valhalla, Nominatim) + companion server TUI
./run.sh
```

Or run without maps (NTP and search only):

```bash
cd companion-server
python3 -m venv .venv && source .venv/bin/activate
pip install -e .
python -m companion_server
```

The TUI will display:
- **Announce Stream**: Devices announcing on the network
- **Trusted Devices**: Devices you've established trust with
- **Log Panel**: Service activity and debug information

See `companion-server/docker/README.md` for region selection, resource requirements, and troubleshooting.

## Trust Workflow

rDeck uses a mutual trust model for security. Both the device and server must explicitly trust each other before services are available.

```
┌─────────────────┐                      ┌─────────────────┐
│ Companion Server│                      │     rDeck       │
├─────────────────┤                      ├─────────────────┤
│                 │  1. Sees announce    │                 │
│  Announce Stream│◄─────────────────────│  Auto-announces │
│                 │                      │                 │
│  2. User clicks │                      │                 │
│     "Trust"     │                      │                 │
│                 │  3. TRUST_OFFER      │                 │
│                 │─────────────────────►│  Pending offer  │
│                 │                      │  appears        │
│                 │                      │                 │
│                 │                      │  4. User opens  │
│                 │                      │  Settings →     │
│                 │  5. TRUST_ACCEPT     │  Trusted Servers│
│  Mutual trust   │◄─────────────────────│  → Accept       │
│  established    │                      │                 │
│                 │                      │                 │
│  Services now   │◄────────────────────►│  Services now   │
│  available      │   NTP, Search, etc.  │  available      │
└─────────────────┘                      └─────────────────┘
```

### On the Companion Server:
1. Launch the TUI (`python -m companion_server`)
2. Wait for rDeck's announce to appear in the stream
3. Select the announce and press Enter or click "Trust"

### On rDeck:
1. Open **Settings** app
2. Navigate to **Trusted Servers**
3. Pending offers appear with server name and services
4. Press **Accept** to establish mutual trust

Once trusted, rDeck will automatically:
- Sync time via NTP on startup
- Enable web search in the WebSearch app
- Enable offline maps with tile fetching, routing, and geocoding

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         RetOS                                │
├──────────────────────────┬──────────────────────────────────┤
│        UI Task           │         Services Task            │
├──────────────────────────┼──────────────────────────────────┤
│  ┌─────────────────┐     │  ┌─────────────────┐             │
│  │    RetUI        │     │  │   RnsService    │             │
│  │  (LVGL-based)   │     │  │  (Reticulum +   │             │
│  └────────┬────────┘     │  │   LXMF + Trust) │             │
│           │              │  └─────────────────┘             │
│  ┌────────┴────────┐     │  ┌─────────────────┐             │
│  │      Apps       │     │  │   GPSService    │             │
│  │ Launcher, UChat │     │  │  (Location +    │             │
│  │ Clock, Notes,   │     │  │   Time sync)    │             │
│  │ Settings, Maps, │     │  └─────────────────┘             │
│  │ WebSearch       │     │  ┌─────────────────┐             │
│  └─────────────────┘     │  │  WifiService    │             │
│                          │  │  TimeService    │             │
│                          │  └─────────────────┘             │
├──────────────────────────┴──────────────────────────────────┤
│                    Hardware Abstraction                      │
│    Screen │ Keyboard │ Battery │ GPS │ LoRa │ Filesystem    │
└─────────────────────────────────────────────────────────────┘
```

### Key Components

| Component | Description |
|-----------|-------------|
| **RetOS** | Core OS managing app/service lifecycle and events |
| **RetUI** | LVGL-based UI with top bar and app screen area |
| **RnsService** | Reticulum identity, LXMF messaging, trust management |
| **GPSService** | GPS coordinates and time synchronization |
| **WifiService** | WiFi connectivity, enables TCP interface as alternative to LoRa |
| **TimeService** | Centralized time management with source priority (GPS > NTP > Manual) |
| **TrustedServers** | Persistent storage of trusted companion servers |

## Apps

| App | Description |
|-----|-------------|
| **Launcher** | App grid home screen |
| **UChat** | LXMF encrypted messaging |
| **Clock** | Time display with timezone support |
| **Notes** | Local note-taking |
| **Maps** | Offline maps with GPS tracking, pan/zoom, geocoding, and routing |
| **Settings** | System settings, trusted servers management |
| **WebSearch** | Web search via companion server |

## Development

### Running Tests

```bash
# Python companion server tests
cd companion-server
python3 -m venv .venv && source .venv/bin/activate
pip install -e ".[dev]"
pytest

# C++ unit tests
pio test -e test_native
```

### Project Structure

```
rDeck/
├── src/
│   ├── apps/           # Application implementations
│   ├── services/       # Background services
│   │   └── RnsUtils/   # Reticulum utilities (TrustedServers, ServiceProtocol)
│   ├── retOS/          # Core OS (RetOS, RetUI, Events, TimeHelper)
│   ├── retHal/         # Hardware abstraction layer
│   └── boards/         # Board-specific code (T-Deck Pro, Emulator)
├── companion-server/   # Python companion server
│   ├── companion_server/
│   │   ├── protocol/   # Message definitions, serialization
│   │   ├── services/   # NTP, Search, Maps services
│   │   └── tui/        # Textual TUI
│   ├── docker/         # Docker setup for maps infrastructure
│   │   ├── setup.sh    # Region selection and data download
│   │   ├── run.sh      # Start services + companion server
│   │   └── docker-compose.yml  # tileserver, Valhalla, Nominatim
│   └── tests/          # Python test suite
├── test/               # C++ unit tests
├── hal/                # SDL2 HAL for emulator
└── lib/                # Local libraries
```

## Protocol

rDeck and the Companion Server communicate using a msgpack-based protocol over LXMF messages.

### Message Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| TRUST_OFFER | 0x01 | Server→Device | Offer to provide services |
| TRUST_ACCEPT | 0x02 | Device→Server | Accept the trust offer |
| TRUST_REVOKE | 0x03 | Either | Revoke established trust |
| NTP_REQUEST | 0x10 | Device→Server | Request current time |
| NTP_RESPONSE | 0x11 | Server→Device | Time response with RTT data |
| SEARCH_REQUEST | 0x20 | Device→Server | Web search query |
| SEARCH_RESPONSE | 0x21 | Server→Device | Search results |
| MAP_TILE_REQUEST | 0x30 | Device→Server | Request map tile (z/x/y) |
| MAP_TILE_RESPONSE | 0x31 | Server→Device | 1-bit dithered tile data (chunked) |
| MAP_ROUTE_REQUEST | 0x33 | Device→Server | Routing between two points |
| MAP_ROUTE_RESPONSE | 0x34 | Server→Device | Route geometry + turn instructions |
| MAP_GEOCODE_REQUEST | 0x35 | Device→Server | Address/place search |
| MAP_GEOCODE_RESPONSE | 0x36 | Server→Device | Geocode results with coordinates |

## Contributing

Contributions are welcome! Please ensure:
- Code compiles for both hardware and emulator targets
- Unit tests pass (`pio test -e test_native` and `pytest`)
- Protocol changes are synchronized between Python and C++ implementations

## License

MIT

## Acknowledgments

- [Reticulum Network Stack](https://reticulum.network/) - The foundation for mesh networking
- [LXMF](https://github.com/markqvist/LXMF) - Lightweight Extensible Message Format
- [microReticulum](https://github.com/attermann/microReticulum) - ESP32 Reticulum implementation
- [LilyGo](https://www.lilygo.cc/) - T-Deck Pro hardware
