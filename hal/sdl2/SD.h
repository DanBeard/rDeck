/**
 * SD.h - SD card stub for emulator.
 * Provides minimal SD card interface for native builds.
 */
#pragma once

#include "FS.h"

// Forward declaration
class SPIClass;

// SD card class (stub - uses regular filesystem)
class SDClass {
public:
    bool begin(uint8_t cs = 0) {
        (void)cs;
        _mounted = true;
        return true;
    }

    void end() {
        _mounted = false;
    }

    bool exists(const char* path) {
        return _fs.exists(path);
    }

    File open(const char* path, const char* mode = "r") {
        return _fs.open(path, mode);
    }

    bool mkdir(const char* path) {
        return _fs.mkdir(path);
    }

    bool remove(const char* path) {
        return _fs.remove(path);
    }

    bool rmdir(const char* path) {
        return _fs.rmdir(path);
    }

    bool rename(const char* pathFrom, const char* pathTo) {
        return ::rename(pathFrom, pathTo) == 0;
    }

    uint64_t totalBytes() { return 16ULL * 1024 * 1024 * 1024; }  // 16GB
    uint64_t usedBytes() { return 0; }

    operator bool() { return _mounted; }

private:
    bool _mounted = false;
    FS _fs;
};

extern SDClass SD;
