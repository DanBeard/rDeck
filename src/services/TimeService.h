#pragma once
#include "BaseService.h"
#include "retOS/retosUtils/TimeHelper.h"

/**
 * TimeService - Centralized time management for rDeck.
 *
 * Coordinates all time sources:
 * - GPS (via GPSService - highest priority)
 * - NTP via Reticulum companion servers
 * - Manual setting (lowest priority)
 *
 * Automatically syncs time from available sources based on priority.
 */
class TimeService : public BaseService {
public:
    using BaseService::BaseService;

    virtual void start(RetOS* retos) override;
    virtual void tick(const unsigned long tickMillis) override;

    /**
     * Request NTP sync from a trusted server.
     * Called automatically, but can be triggered manually.
     */
    void requestNtpSync();

    /**
     * Handle NTP response from companion server.
     * Called by RnsService when it receives an NTP_RESPONSE.
     */
    void handleNtpResponse(uint32_t serverTimestamp, uint32_t clientTimestamp);

    /**
     * Check if NTP sync is needed (no GPS and time source is lower priority).
     */
    bool needsNtpSync() const;

private:
    // Aggressive sync on boot until we get good time, then relax
    static constexpr unsigned long NTP_SYNC_INTERVAL_FAST = 1 * 60 * 1000;   // 1 minute (before time is set)
    static constexpr unsigned long NTP_SYNC_INTERVAL_SLOW = 10 * 60 * 1000;  // 10 minutes (after time is set)
    static constexpr unsigned long NTP_REQUEST_TIMEOUT = 30 * 1000;          // 30 seconds

    unsigned long _lastNtpSync = 0;
    unsigned long _lastNtpRequest = 0;
    uint32_t _pendingNtpRequestId = 0;
    bool _ntpRequestPending = false;
    bool _timeSetSinceBoot = false;  // True once we've received good time from any source

    unsigned long getSyncInterval() const;
};
