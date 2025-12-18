/**
 * Arduino.h compatibility layer for native/emulator builds.
 * Provides stubs for common Arduino functions using SDL2.
 */
#pragma once

#ifndef ARDUINO
#define ARDUINO 100  // Fake Arduino version for compatibility checks
#endif

// Include C++ headers only when compiling C++
#ifdef __cplusplus
// Include chrono and mutex FIRST, before any "using namespace std;" pollutes things
// Some source files have "using namespace std;" which causes std::chrono to be
// pulled into global scope, creating ambiguity with the chrono namespace inside std::
#include <chrono>
#include <mutex>
#include <thread>
#endif

#ifdef __cplusplus
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <string>
#else
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#endif

#include <SDL2/SDL.h>

// Arduino types
typedef uint8_t byte;
#ifdef __cplusplus
typedef bool boolean;
#else
typedef int boolean;
#define true 1
#define false 0
#endif

// PROGMEM support (AVR/ESP32 program memory - no-op on native)
#define PROGMEM
#define PGM_P const char*
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#define pgm_read_word(addr) (*(const uint16_t*)(addr))
#define pgm_read_dword(addr) (*(const uint32_t*)(addr))
#define pgm_read_float(addr) (*(const float*)(addr))
#define pgm_read_ptr(addr) (*(const void**)(addr))
#define memcpy_P memcpy
#define strcmp_P strcmp
#define strcpy_P strcpy
#define strncpy_P strncpy
#define strlen_P strlen

#ifdef __cplusplus
// F() macro for flash strings (no-op on native) - C++ only
class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper*>(PSTR(string_literal)))
#else
// C version - just returns the string literal
#define F(string_literal) (string_literal)
#endif

// Timing functions - these need to be available in both C and C++
#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t millis(void) {
    return SDL_GetTicks();
}

static inline uint32_t micros(void) {
    return SDL_GetTicks() * 1000;
}

static inline void delay(uint32_t ms) {
    SDL_Delay(ms);
}

static inline void delayMicroseconds(uint32_t us) {
    SDL_Delay(us / 1000);
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// All C++ classes and code below this point

// Serial class stub
class SerialClass {
public:
    void begin(int baud) { (void)baud; }
    void end() {}

    // Write methods for ArduinoJson compatibility
    size_t write(uint8_t c) { putchar(c); return 1; }
    size_t write(const uint8_t* buf, size_t size) {
        for (size_t i = 0; i < size; i++) putchar(buf[i]);
        return size;
    }
    size_t write(const char* str) {
        if (!str) return 0;
        size_t len = strlen(str);
        for (size_t i = 0; i < len; i++) putchar(str[i]);
        return len;
    }

    void print(const char* s) { printf("%s", s); }
    void print(const __FlashStringHelper* s) { print(reinterpret_cast<const char*>(s)); }
    void print(char c) { printf("%c", c); }
    void print(int v) { printf("%d", v); }
    void print(unsigned int v) { printf("%u", v); }
    void print(long v) { printf("%ld", v); }
    void print(unsigned long v) { printf("%lu", v); }
    void print(double v, int digits = 2) { printf("%.*f", digits, v); }

    void println() { printf("\n"); }
    void println(const char* s) { printf("%s\n", s); }
    void println(const __FlashStringHelper* s) { println(reinterpret_cast<const char*>(s)); }
    void println(char c) { printf("%c\n", c); }
    void println(int v) { printf("%d\n", v); }
    void println(unsigned int v) { printf("%u\n", v); }
    void println(long v) { printf("%ld\n", v); }
    void println(unsigned long v) { printf("%lu\n", v); }
    void println(double v, int digits = 2) { printf("%.*f\n", digits, v); }

    void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }

    int available() { return 0; }
    int read() { return -1; }
    void flush() { fflush(stdout); }

    operator bool() { return true; }
};

extern SerialClass Serial;

// Forward declarations
class String;
class Print;
class Stream;
class Printable;

// Print base class
#ifndef ARDUINO_PRINT_CLASS
#define ARDUINO_PRINT_CLASS
class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t* buf, size_t size) {
        size_t n = 0;
        while (size--) n += write(*buf++);
        return n;
    }
    size_t write(const char* str) {
        if (!str) return 0;
        return write((const uint8_t*)str, strlen(str));
    }
    size_t print(const char* s) { return write(s); }
    size_t print(const __FlashStringHelper* s) { return print(reinterpret_cast<const char*>(s)); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(int v) { char buf[32]; snprintf(buf, sizeof(buf), "%d", v); return write(buf); }
    size_t print(unsigned int v) { char buf[32]; snprintf(buf, sizeof(buf), "%u", v); return write(buf); }
    size_t print(long v) { char buf[32]; snprintf(buf, sizeof(buf), "%ld", v); return write(buf); }
    size_t print(unsigned long v) { char buf[32]; snprintf(buf, sizeof(buf), "%lu", v); return write(buf); }
    size_t print(double v, int prec = 2) { char buf[32]; snprintf(buf, sizeof(buf), "%.*f", prec, v); return write(buf); }
    size_t println() { return write("\n"); }
    size_t println(const char* s) { return print(s) + println(); }
    size_t println(const __FlashStringHelper* s) { return print(s) + println(); }
    size_t println(char c) { return print(c) + println(); }
    size_t println(int v) { return print(v) + println(); }
    size_t println(unsigned int v) { return print(v) + println(); }
    size_t println(long v) { return print(v) + println(); }
    size_t println(unsigned long v) { return print(v) + println(); }
    size_t println(double v, int prec = 2) { return print(v, prec) + println(); }
    void flush() {}
};
#endif // ARDUINO_PRINT_CLASS

// Printable interface
class Printable {
public:
    virtual size_t printTo(Print& p) const = 0;
};

// String class - Arduino-compatible implementation
class String : public Printable {
public:
    String() : _str("") {}
    String(const char* s) : _str(s ? s : "") {}
    String(char c) : _str(1, c) {}
    String(int v) { char buf[32]; snprintf(buf, sizeof(buf), "%d", v); _str = buf; }
    String(unsigned int v) { char buf[32]; snprintf(buf, sizeof(buf), "%u", v); _str = buf; }
    String(long v) { char buf[64]; snprintf(buf, sizeof(buf), "%ld", v); _str = buf; }
    String(unsigned long v) { char buf[64]; snprintf(buf, sizeof(buf), "%lu", v); _str = buf; }
    String(double v, int prec = 2) { char buf[64]; snprintf(buf, sizeof(buf), "%.*f", prec, v); _str = buf; }
    String(const String& other) : _str(other._str) {}
    String(const __FlashStringHelper* s) : _str(reinterpret_cast<const char*>(s)) {}

    const char* c_str() const { return _str.c_str(); }
    size_t length() const { return _str.length(); }
    bool isEmpty() const { return _str.empty(); }

    String& operator=(const char* s) { _str = s ? s : ""; return *this; }
    String& operator=(const String& other) { _str = other._str; return *this; }
    String& operator+=(const char* s) { if (s) _str += s; return *this; }
    String& operator+=(const String& other) { _str += other._str; return *this; }
    String& operator+=(char c) { _str += c; return *this; }

    String operator+(const char* s) const { return String((_str + (s ? s : "")).c_str()); }
    String operator+(const String& other) const { return String((_str + other._str).c_str()); }

    bool operator==(const char* s) const { return _str == (s ? s : ""); }
    bool operator==(const String& other) const { return _str == other._str; }
    bool operator!=(const char* s) const { return !(*this == s); }
    bool operator!=(const String& other) const { return !(*this == other); }

    char operator[](size_t i) const { return _str[i]; }
    char& operator[](size_t i) { return _str[i]; }

    // Concat methods for ArduinoJson
    bool concat(const char* s) { if (s) _str += s; return true; }
    bool concat(const String& other) { _str += other._str; return true; }
    bool concat(char c) { _str += c; return true; }
    bool concat(const char* buf, size_t len) { _str.append(buf, len); return true; }

    int indexOf(char c) const {
        auto pos = _str.find(c);
        return pos == std::string::npos ? -1 : (int)pos;
    }
    int indexOf(const char* s) const {
        auto pos = _str.find(s);
        return pos == std::string::npos ? -1 : (int)pos;
    }

    String substring(size_t from) const { return String(_str.substr(from).c_str()); }
    String substring(size_t from, size_t to) const { return String(_str.substr(from, to - from).c_str()); }

    void toCharArray(char* buf, size_t len) const { strncpy(buf, _str.c_str(), len); }
    int toInt() const { return atoi(_str.c_str()); }
    float toFloat() const { return (float)atof(_str.c_str()); }

    void reserve(size_t size) { _str.reserve(size); }
    void remove(size_t idx) { if (idx < _str.length()) _str.erase(idx); }
    void remove(size_t idx, size_t count) { if (idx < _str.length()) _str.erase(idx, count); }
    void toLowerCase() { for (auto& c : _str) c = tolower(c); }
    void toUpperCase() { for (auto& c : _str) c = toupper(c); }
    void trim() {
        size_t start = _str.find_first_not_of(" \t\n\r");
        size_t end = _str.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) _str.clear();
        else _str = _str.substr(start, end - start + 1);
    }

    // Printable interface
    virtual size_t printTo(Print& p) const override {
        return p.write(_str.c_str());
    }

private:
    std::string _str;
};

// StringSumHelper - used by Arduino for string concatenation with +
class StringSumHelper : public String {
public:
    StringSumHelper(const String& s) : String(s) {}
    StringSumHelper(const char* s) : String(s) {}
    StringSumHelper(char c) : String(c) {}
    StringSumHelper(int v) : String(v) {}
    StringSumHelper(unsigned int v) : String(v) {}
    StringSumHelper(long v) : String(v) {}
    StringSumHelper(unsigned long v) : String(v) {}
    StringSumHelper(double v, int prec = 2) : String(v, prec) {}
};

// Allow String + operations to work
inline StringSumHelper operator+(const String& lhs, const char* rhs) {
    StringSumHelper result(lhs);
    result += rhs;
    return result;
}
inline StringSumHelper operator+(const String& lhs, const String& rhs) {
    StringSumHelper result(lhs);
    result += rhs;
    return result;
}
inline StringSumHelper operator+(const String& lhs, char c) {
    StringSumHelper result(lhs);
    result += c;
    return result;
}

// Stream base class (defined after String)
#ifndef ARDUINO_STREAM_CLASS
#define ARDUINO_STREAM_CLASS
class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;

    // Default implementations
    size_t readBytes(char* buf, size_t len) {
        size_t count = 0;
        while (count < len) {
            int c = read();
            if (c < 0) break;
            *buf++ = (char)c;
            count++;
        }
        return count;
    }
    String readString() {
        String ret;
        int c;
        while ((c = read()) >= 0) ret += (char)c;
        return ret;
    }

    // Find methods
    bool find(const char* target) {
        if (!target) return false;
        size_t targetLen = strlen(target);
        if (targetLen == 0) return true;
        size_t index = 0;
        while (true) {
            int c = read();
            if (c < 0) return false;
            if ((char)c == target[index]) {
                index++;
                if (index == targetLen) return true;
            } else {
                index = 0;
            }
        }
    }

    bool find(char target) {
        while (true) {
            int c = read();
            if (c < 0) return false;
            if ((char)c == target) return true;
        }
    }

    size_t readBytesUntil(char terminator, char* buffer, size_t length) {
        size_t count = 0;
        while (count < length) {
            int c = read();
            if (c < 0 || (char)c == terminator) break;
            buffer[count++] = (char)c;
        }
        return count;
    }

    size_t readBytesUntil(char terminator, uint8_t* buffer, size_t length) {
        return readBytesUntil(terminator, (char*)buffer, length);
    }
};
#endif // ARDUINO_STREAM_CLASS

// GPIO stubs (no-ops)
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1

inline void pinMode(int pin, int mode) { (void)pin; (void)mode; }
inline void digitalWrite(int pin, int value) { (void)pin; (void)value; }
inline int digitalRead(int pin) { (void)pin; return LOW; }
inline int analogRead(int pin) { (void)pin; return 0; }
inline void analogWrite(int pin, int value) { (void)pin; (void)value; }

// SPI stubs
#define SPI_MODE0 0
#define MSBFIRST 1

class SPISettings {
public:
    SPISettings() {}
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
        (void)clock; (void)bitOrder; (void)dataMode;
    }
};

class SPIClass {
public:
    void begin() {}
    void end() {}
    void beginTransaction(SPISettings settings) { (void)settings; }
    void endTransaction() {}
    uint8_t transfer(uint8_t data) { (void)data; return 0; }
    void transfer(void* buf, size_t count) { (void)buf; (void)count; }
};

extern SPIClass SPI;

// Wire (I2C) stubs
class TwoWire {
public:
    void begin() {}
    void begin(int sda, int scl) { (void)sda; (void)scl; }
    void beginTransmission(uint8_t address) { (void)address; }
    uint8_t endTransmission() { return 0; }
    uint8_t requestFrom(uint8_t address, uint8_t quantity) { (void)address; (void)quantity; return 0; }
    size_t write(uint8_t data) { (void)data; return 1; }
    int available() { return 0; }
    int read() { return -1; }
};

extern TwoWire Wire;

// Math functions (using templates instead of macros to avoid conflicts)
#include <algorithm>
using std::min;
using std::max;

template<typename T>
inline T constrain(T amt, T low, T high) {
    return (amt < low) ? low : ((amt > high) ? high : amt);
}

#ifndef abs
#define abs(x) ((x)>0?(x):-(x))
#endif

// Memory allocation (standard malloc)
inline void* ps_malloc(size_t size) { return malloc(size); }
inline void* ps_calloc(size_t num, size_t size) { return calloc(num, size); }

// Random functions (Arduino compatible)
inline long random(long max) {
    if (max == 0) return 0;
    return rand() % max;
}

inline long random(long min, long max) {
    if (min >= max) return min;
    return min + (rand() % (max - min));
}

inline void randomSeed(unsigned long seed) {
    srand((unsigned int)seed);
}

// itoa is not standard C/C++, but some Arduino code uses it
inline char* itoa(int value, char* str, int base) {
    if (base == 10) {
        sprintf(str, "%d", value);
    } else if (base == 16) {
        sprintf(str, "%x", value);
    } else if (base == 8) {
        sprintf(str, "%o", value);
    } else if (base == 2) {
        // Binary conversion
        char* p = str;
        if (value == 0) {
            *p++ = '0';
        } else {
            int v = value;
            char tmp[33];
            int i = 0;
            while (v > 0) {
                tmp[i++] = '0' + (v & 1);
                v >>= 1;
            }
            while (i > 0) *p++ = tmp[--i];
        }
        *p = '\0';
    } else {
        sprintf(str, "%d", value);
    }
    return str;
}

inline char* ltoa(long value, char* str, int base) {
    if (base == 10) {
        sprintf(str, "%ld", value);
    } else if (base == 16) {
        sprintf(str, "%lx", value);
    } else {
        sprintf(str, "%ld", value);
    }
    return str;
}

inline char* utoa(unsigned int value, char* str, int base) {
    if (base == 10) {
        sprintf(str, "%u", value);
    } else if (base == 16) {
        sprintf(str, "%x", value);
    } else {
        sprintf(str, "%u", value);
    }
    return str;
}

inline char* ultoa(unsigned long value, char* str, int base) {
    if (base == 10) {
        sprintf(str, "%lu", value);
    } else if (base == 16) {
        sprintf(str, "%lx", value);
    } else {
        sprintf(str, "%lu", value);
    }
    return str;
}

#endif // __cplusplus
