#include "TimeService.h"
#include "RnsService.h"
#include "RnsUtils/TrustedServers.h"

void TimeService::start(RetOS* retos) {
    Serial.println("[TimeService] Starting...");

    // Check if time was already set (e.g., from persisted state or early GPS)
    _timeSetSinceBoot = (_retos->time.getTimeSource() != TimeSource::NONE);
    if (_timeSetSinceBoot) {
        Serial.println("[TimeService] Time already set, using slow sync interval");
    } else {
        Serial.println("[TimeService] No time set yet, using fast sync interval (1 min)");
    }

    _status = RUNNING;
}

unsigned long TimeService::getSyncInterval() const {
    return _timeSetSinceBoot ? NTP_SYNC_INTERVAL_SLOW : NTP_SYNC_INTERVAL_FAST;
}

void TimeService::tick(const unsigned long tMillis) {
    // Check if time source changed (e.g., GPS came online)
    if (!_timeSetSinceBoot && _retos->time.getTimeSource() != TimeSource::NONE) {
        _timeSetSinceBoot = true;
        Serial.println("[TimeService] Time now set, switching to slow sync interval (10 min)");
    }

    // Check if we need to sync time
    unsigned long interval = getSyncInterval();
    if (tMillis - _lastNtpSync > interval || tMillis < _lastNtpSync) {
        if (needsNtpSync()) {
            requestNtpSync();
            _lastNtpSync = tMillis;  // Update even if no server found to avoid spam
        }
    }

    // Check for NTP request timeout
    if (_ntpRequestPending && (tMillis - _lastNtpRequest > NTP_REQUEST_TIMEOUT)) {
        Serial.println("[TimeService] NTP request timed out");
        _ntpRequestPending = false;
    }

    // Propagation sync
    if (tMillis - _lastPropSync > PROP_SYNC_INTERVAL || tMillis < _lastPropSync) {
        requestPropSync();
        _lastPropSync = tMillis;
    }
}

bool TimeService::needsNtpSync() const {
    // Don't sync if GPS is providing time (highest priority)
    if (_retos->time.getTimeSource() == TimeSource::GPS) {
        return false;
    }

    // Don't sync if we have a pending request
    if (_ntpRequestPending) {
        return false;
    }

    return true;
}

void TimeService::requestNtpSync() {
    // Find a trusted server that offers NTP
    auto trustedServers = Retcon::Service::getTrustedServers().getTrustedServers();

    for (const auto& server : trustedServers) {
        for (const auto& svc : server.services) {
            if (svc == "ntp") {
                // Found an NTP server, request sync via RnsService
                RnsService* rns = _retos->fetchService<RnsService>();
                if (rns) {
                    Serial.printf("[TimeService] Requesting NTP sync from '%s'\n", server.name.c_str());
                    rns->requestNtpSync(server.hash);
                    _lastNtpRequest = millis();
                    _ntpRequestPending = true;
                    return;
                }
            }
        }
    }

    // No NTP server found - that's OK, we'll try again later
}

void TimeService::requestPropSync() {
    auto trustedServers = Retcon::Service::getTrustedServers().getTrustedServers();

    for (const auto& server : trustedServers) {
        for (const auto& svc : server.services) {
            if (svc == "propagation") {
                RnsService* rns = _retos->fetchService<RnsService>();
                if (rns) {
                    Serial.printf("[TimeService] Requesting propagation sync from '%s'\n", server.name.c_str());
                    rns->requestPropSync(server.hash);
                    return;
                }
            }
        }
    }
}

void TimeService::handleNtpResponse(uint32_t serverTimestamp, uint32_t clientTimestamp) {
    Serial.printf("[TimeService] Received NTP response: server_time=%u, client_time=%u\n",
                  serverTimestamp, clientTimestamp);

    _ntpRequestPending = false;

    // Calculate RTT
    uint32_t now = millis();
    uint32_t rtt = now - _lastNtpRequest;
    Serial.printf("[TimeService] RTT: %u ms\n", rtt);

    // Apply time with RTT compensation (assume symmetric latency)
    time_t adjusted_time = serverTimestamp + (rtt / 2000);  // Convert ms to seconds

    // Set time using TimeHelper with RETICULUM_NTP source
    if (_retos->time.setTime(adjusted_time, TimeSource::RETICULUM_NTP)) {
        Serial.printf("[TimeService] Time synchronized to %lu (adjusted for RTT)\n",
                      (unsigned long)adjusted_time);
    } else {
        Serial.println("[TimeService] Time update rejected (higher priority source active)");
    }
}
