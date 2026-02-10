#pragma once

/**
 * PlatformMutex - Cross-platform mutex abstraction.
 *
 * Uses std::mutex on emulator (native platform).
 * Uses FreeRTOS SemaphoreHandle_t on ESP32.
 */

#ifdef RET_PLATFORM_EMU

#include <chrono>
#include <mutex>

class PlatformMutex {
public:
    PlatformMutex() = default;
    ~PlatformMutex() = default;

    void lock() {
        _mutex.lock();
    }

    void unlock() {
        _mutex.unlock();
    }

    bool tryLock() {
        return _mutex.try_lock();
    }

private:
    std::mutex _mutex;
};

// RAII lock guard
class PlatformMutexGuard {
public:
    explicit PlatformMutexGuard(PlatformMutex& mutex) : _mutex(mutex) {
        _mutex.lock();
    }
    ~PlatformMutexGuard() {
        // Catch exceptions in destructor to prevent std::terminate during
        // stack unwinding. try-catch has zero runtime cost on x86_64
        // (zero-cost exception model) when no exception is thrown.
        try {
            _mutex.unlock();
        } catch (...) {}
    }

    // Non-copyable
    PlatformMutexGuard(const PlatformMutexGuard&) = delete;
    PlatformMutexGuard& operator=(const PlatformMutexGuard&) = delete;

private:
    PlatformMutex& _mutex;
};

#else  // ESP32 / FreeRTOS

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class PlatformMutex {
public:
    PlatformMutex() {
        _sem = xSemaphoreCreateMutex();
    }

    ~PlatformMutex() {
        if (_sem) {
            vSemaphoreDelete(_sem);
        }
    }

    void lock() {
        xSemaphoreTake(_sem, portMAX_DELAY);
    }

    void unlock() {
        xSemaphoreGive(_sem);
    }

    bool tryLock() {
        return xSemaphoreTake(_sem, 0) == pdTRUE;
    }

    // For backward compatibility - get raw handle
    SemaphoreHandle_t handle() const { return _sem; }

private:
    SemaphoreHandle_t _sem = nullptr;
};

// RAII lock guard
class PlatformMutexGuard {
public:
    explicit PlatformMutexGuard(PlatformMutex& mutex) : _mutex(mutex) {
        _mutex.lock();
    }
    ~PlatformMutexGuard() {
        _mutex.unlock();
    }

    // Non-copyable
    PlatformMutexGuard(const PlatformMutexGuard&) = delete;
    PlatformMutexGuard& operator=(const PlatformMutexGuard&) = delete;

private:
    PlatformMutex& _mutex;
};

#endif
