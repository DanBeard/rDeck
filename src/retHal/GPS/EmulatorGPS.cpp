#ifdef RET_PLATFORM_EMU

#include "EmulatorGPS.h"
#include <cstdio>

static TinyGPSPlus fakeGps;

void EmulatorGPS::initGPS() {
    // Configure simulated position: Sears Tower, Chicago
    fakeGps.location.setLocation(41.8789, -87.6359);
    fakeGps.course.setCourse(45.0);
    fakeGps.date.setDate(2026, 2, 10);
    fakeGps.time.setTime(12, 0, 0);

    GPS = &fakeGps;
    printf("[EmulatorGPS] Initialized with simulated position (41.8789, -87.6359)\n");
}

void EmulatorGPS::tick() {
    // No-op - simulated data is static
}

#endif
