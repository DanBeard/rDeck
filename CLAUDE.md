# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

rDeck is a custom operating system (RetOS) for the LilyGo T-Deck Pro hardware - an ESP32-S3 based handheld device with e-paper display, keyboard, LoRa radio, and GPS. It implements the Reticulum Network Stack for off-grid mesh communication via LXMF messaging.

### Vision

rDeck aims to be a Reticulum-first off-grid smartphone replacement. The Companion Server system extends its capabilities by providing infrastructure services (NTP, web search) to trusted devices over the mesh network, enabling smartphone-like functionality without internet connectivity on the device itself.

## Build Commands

```bash
# Build for T-Deck Pro hardware (default)
pio run -e T-Deck-Pro

# Build for desktop emulator (SDL2)
pio run -e emulator_64bits

# Upload to device
pio run -e T-Deck-Pro --target upload

# Monitor serial output
pio device monitor
```

## Architecture

### Core System (`src/retOS/`)

- **RetOS** (`retOS.h/cpp`) - Main OS class, manages app/service lifecycle, event routing, and dual-task architecture (UI task + services task)
- **RetUI** (`retUI.h/cpp`) - LVGL-based UI framework with top bar (battery, service icons, back button) and app screen area
- **Events** (`Events.h`) - Event system for inter-component communication (GPS, messages, time changes, etc.)
- **RetRunnable** (`RetRunnable.h`) - Base interface for tickable components (apps and services)

### Hardware Abstraction (`src/retHal/`)

- **RetHal** (`RetHal.h`) - HAL struct containing pointers to all hardware drivers (screen, keyboard, battery, GPS, LoRa, filesystem)
- Platform-specific implementations in `Screen/`, `Keyboard/`, `Battery/`, `GPS/`, `Lora/` subdirectories
- Base classes define interfaces, T-Deck Pro implementations are the concrete drivers

### Board Support (`src/boards/`)

- **TDeckPro.h/cpp** - T-Deck Pro hardware configuration and initialization
- **Emulator.h/cpp** - SDL2-based desktop emulator for development
- Selected at compile time via `RET_PLATFORM_TDECKPRO` or `RET_PLATFORM_EMU` defines

### Apps (`src/apps/`)

Inherit from `BaseApp`. Single active app at a time. Lifecycle: construct → `startApp()` → `tick()` loop → `stop()` → destroy.

Key apps: Launcher (app grid), UChat (LXMF messaging), Clock, Notes, Settings, WebSearch

Register new apps in `rdeck.ino` using `AppFactory<YourApp>("Name", &icon)`

### Services (`src/services/`)

Inherit from `BaseService`. Run continuously in background. Start once and persist.

- **RnsService** - Reticulum Network Stack integration (identity, LoRa interface, LXMF messaging)
- **GPSService** - GPS data processing and time sync

Register new services in `rdeck.ino` using `ServiceFactory<YourService>()`

### Key Patterns

- **Factory registration**: Apps/services use template factories that auto-assign IDs:
  ```cpp
  AppFactory<MyApp>("Name", &icon)  // in rdeck.ino apps list
  ServiceFactory<MyService>()       // in rdeck.ino services list
  ```

- **Fetching services**: `retos->fetchService<RnsService>()` returns typed pointer

- **Event publishing**: Services call `publishEvent(Event{...})` to notify other components

- **UI drawing**: Apps draw to `screen` (lv_obj_t*), use `_retos->ui()` for theme colors

### Companion Server (`companion-server/`)

A Python-based companion server that provides infrastructure services to rDeck devices over Reticulum:

- **NTP Service**: Time synchronization over mesh
- **Search Service**: Web search proxy via DuckDuckGo
- **Trust Management**: Mutual trust workflow for secure service access

See `companion-server/README.md` for setup and usage.

#### Trust Workflow

1. rDeck announces on network
2. Server user clicks "Trust" in TUI
3. Server sends TRUST_OFFER message
4. rDeck user accepts in Settings → Trusted Servers
5. rDeck sends TRUST_ACCEPT
6. Services now available

#### Service Protocol

Messages use LXMF fields with msgpack encoding. Key message types:

| Type | Value | Direction | Purpose |
|------|-------|-----------|---------|
| TRUST_OFFER | 0x01 | Server→Device | Trust offer |
| TRUST_ACCEPT | 0x02 | Device→Server | Accept trust |
| NTP_REQUEST/RESPONSE | 0x10/0x11 | Bidirectional | Time sync |
| SEARCH_REQUEST/RESPONSE | 0x20/0x21 | Bidirectional | Web search |

### Dependencies

- LVGL 8.3.x for UI
- microReticulum (symlinked from `../microReticulum`) for Reticulum Network Stack
- RadioLib for LoRa
- Local libraries in `lib/` (GxEPD2, TinyGPSPlus, XPowersLib, SensorLib, etc.)
