#ifdef RET_PLATFORM_EMU

#include "EmulatorGPS.h"
#include <cstdio>

void EmulatorGPS::initGPS() {
    printf("[EmulatorGPS] Initialized (no GPS in emulator)\n");
    GPS = nullptr;  // No GPS hardware in emulator
}

void EmulatorGPS::tick() {
    // No-op - no GPS data to process
}

#endif
