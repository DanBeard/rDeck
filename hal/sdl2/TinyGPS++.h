/**
 * TinyGPS++.h - Stub GPS library for emulator.
 * Provides empty classes so code compiles without actual GPS hardware.
 */
#pragma once

#include <cstdint>

// Stub GPS data structure
struct RawDegrees {
    int16_t deg = 0;
    uint32_t billionths = 0;
    bool negative = false;
};

class TinyGPSLocation {
public:
    bool isValid() const { return false; }
    bool isUpdated() const { return false; }
    double lat() const { return 0.0; }
    double lng() const { return 0.0; }
    RawDegrees rawLat() const { return RawDegrees(); }
    RawDegrees rawLng() const { return RawDegrees(); }
    uint32_t age() const { return 0xFFFFFFFF; }
};

class TinyGPSDate {
public:
    bool isValid() const { return false; }
    bool isUpdated() const { return false; }
    uint32_t value() const { return 0; }
    uint16_t year() const { return 0; }
    uint8_t month() const { return 0; }
    uint8_t day() const { return 0; }
    uint32_t age() const { return 0xFFFFFFFF; }
};

class TinyGPSTime {
public:
    bool isValid() const { return false; }
    bool isUpdated() const { return false; }
    uint32_t value() const { return 0; }
    uint8_t hour() const { return 0; }
    uint8_t minute() const { return 0; }
    uint8_t second() const { return 0; }
    uint8_t centisecond() const { return 0; }
    uint32_t age() const { return 0xFFFFFFFF; }
};

class TinyGPSSpeed {
public:
    bool isValid() const { return false; }
    bool isUpdated() const { return false; }
    double value() const { return 0.0; }
    double knots() const { return 0.0; }
    double mph() const { return 0.0; }
    double mps() const { return 0.0; }
    double kmph() const { return 0.0; }
    uint32_t age() const { return 0xFFFFFFFF; }
};

class TinyGPSCourse {
public:
    bool isValid() const { return false; }
    bool isUpdated() const { return false; }
    double value() const { return 0.0; }
    double deg() const { return 0.0; }
    uint32_t age() const { return 0xFFFFFFFF; }
};

class TinyGPSAltitude {
public:
    bool isValid() const { return false; }
    bool isUpdated() const { return false; }
    double value() const { return 0.0; }
    double meters() const { return 0.0; }
    double miles() const { return 0.0; }
    double kilometers() const { return 0.0; }
    double feet() const { return 0.0; }
    uint32_t age() const { return 0xFFFFFFFF; }
};

class TinyGPSInteger {
public:
    bool isValid() const { return false; }
    bool isUpdated() const { return false; }
    uint32_t value() const { return 0; }
    uint32_t age() const { return 0xFFFFFFFF; }
};

class TinyGPSHDOP {
public:
    bool isValid() const { return false; }
    bool isUpdated() const { return false; }
    int32_t value() const { return 0; }
    double hdop() const { return 0.0; }
    uint32_t age() const { return 0xFFFFFFFF; }
};

class TinyGPSPlus {
public:
    bool encode(char c) { (void)c; return false; }

    TinyGPSLocation location;
    TinyGPSDate date;
    TinyGPSTime time;
    TinyGPSSpeed speed;
    TinyGPSCourse course;
    TinyGPSAltitude altitude;
    TinyGPSInteger satellites;
    TinyGPSHDOP hdop;

    uint32_t charsProcessed() const { return 0; }
    uint32_t sentencesWithFix() const { return 0; }
    uint32_t failedChecksum() const { return 0; }
    uint32_t passedChecksum() const { return 0; }
};
