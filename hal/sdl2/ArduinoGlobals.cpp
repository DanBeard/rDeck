/**
 * ArduinoGlobals.cpp - Global instances for Arduino compatibility stubs.
 * Single source file for all global instances to avoid multiple definitions.
 */

#include "Arduino.h"
#include "FS.h"
#include "SD.h"

// Global Arduino-compatible objects
SerialClass Serial;
SPIClass SPI;
TwoWire Wire;

// Global filesystem instances
// Base path is the emulator's data directory
FS SPIFFS("./emu_data");
SDClass SD;
