/**
 * TinyGPS++.h - Stub GPS library for emulator.
 * Provides mutable classes so emulator can simulate GPS data.
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
    bool isValid() const { return _valid; }
    bool isUpdated() const { return _valid; }
    double lat() const { return _lat; }
    double lng() const { return _lng; }
    RawDegrees rawLat() const { return RawDegrees(); }
    RawDegrees rawLng() const { return RawDegrees(); }
    uint32_t age() const { return _valid ? 0 : 0xFFFFFFFF; }

    void setLocation(double lat, double lng) { _lat = lat; _lng = lng; _valid = true; }

private:
    double _lat = 0.0;
    double _lng = 0.0;
    bool _valid = false;
};

class TinyGPSDate {
public:
    bool isValid() const { return _valid; }
    bool isUpdated() const { return _valid; }
    uint32_t value() const { return _value; }
    uint16_t year() const { return _year; }
    uint8_t month() const { return _month; }
    uint8_t day() const { return _day; }
    uint32_t age() const { return _valid ? 0 : 0xFFFFFFFF; }

    void setDate(uint16_t year, uint8_t month, uint8_t day) {
        _year = year; _month = month; _day = day;
        _value = (uint32_t)day + (uint32_t)month * 100 + (uint32_t)year * 10000;
        _valid = true;
    }

private:
    uint16_t _year = 0;
    uint8_t _month = 0;
    uint8_t _day = 0;
    uint32_t _value = 0;
    bool _valid = false;
};

class TinyGPSTime {
public:
    bool isValid() const { return _valid; }
    bool isUpdated() const { return _valid; }
    uint32_t value() const { return _value; }
    uint8_t hour() const { return _hour; }
    uint8_t minute() const { return _minute; }
    uint8_t second() const { return _second; }
    uint8_t centisecond() const { return _centisecond; }
    uint32_t age() const { return _valid ? 0 : 0xFFFFFFFF; }

    void setTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t cs = 0) {
        _hour = hour; _minute = minute; _second = second; _centisecond = cs;
        _value = (uint32_t)second + (uint32_t)minute * 100 + (uint32_t)hour * 10000;
        _valid = true;
    }

private:
    uint8_t _hour = 0;
    uint8_t _minute = 0;
    uint8_t _second = 0;
    uint8_t _centisecond = 0;
    uint32_t _value = 0;
    bool _valid = false;
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
    bool isValid() const { return _valid; }
    bool isUpdated() const { return _valid; }
    double value() const { return _deg * 100.0; }
    double deg() const { return _deg; }
    uint32_t age() const { return _valid ? 0 : 0xFFFFFFFF; }

    void setCourse(double deg) { _deg = deg; _valid = true; }

private:
    double _deg = 0.0;
    bool _valid = false;
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
