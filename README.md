# rDeck

A Reticulum-first off-grid smartphone replacement built on the LilyGo T-Deck Pro.

## Vision

rDeck transforms the T-Deck Pro hardware into an off-grid communication device using the Reticulum Network Stack. With a companion server running on an internet-connected machine, rDeck gains smartphone-like capabilities - time synchronization, web search, and more - all delivered over the encrypted mesh network.

## Features

- **Off-grid Messaging**: LXMF messaging over LoRa mesh
- **Companion Server Integration**: NTP time sync, web search via trusted servers
- **E-paper Display**: Low power, daylight readable
- **GPS**: Location tracking and time synchronization
- **Desktop Emulator**: Develop and test without hardware

## Hardware

- LilyGo T-Deck Pro
- ESP32-S3 processor
- E-paper display (EPD)
- QWERTY keyboard
- LoRa radio (SX1262)
- GPS module

## Quick Start

### Building for Hardware

```bash
pio run -e T-Deck-Pro
pio run -e T-Deck-Pro --target upload
```

### Building the Emulator

```bash
pio run -e emulator_64bits
.pio/build/emulator_64bits/program
```

### Running the Companion Server

```bash
cd companion-server
pip install -e .
python -m companion_server
```

## Companion Server

The companion server runs on an internet-connected machine and provides services to trusted rDeck devices over Reticulum:

- **NTP Service**: Time synchronization
- **Search Service**: Web search via DuckDuckGo proxy

### Trust Workflow

1. rDeck announces itself on the network
2. In the companion server TUI, click "Trust" on the rDeck's announce
3. Server sends a trust offer to rDeck
4. On rDeck, open Settings → Trusted Servers → Accept the offer
5. Services are now available

## Architecture

- **RetOS**: Custom OS with app/service lifecycle management
- **Apps**: Clock, Notes, UChat (messaging), WebSearch, Settings
- **Services**: RnsService (Reticulum/LXMF), GPSService

## License

MIT
