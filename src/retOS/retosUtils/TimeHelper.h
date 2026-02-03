#pragma once
#include <string>
#include <sys/time.h>

/**
 * Time source priority for determining which time updates to accept.
 * Higher values = higher priority = more trusted source.
 */
enum class TimeSource : uint8_t {
    NONE = 0,          // No source set yet
    MANUAL = 1,        // User manually set time
    RETICULUM_NTP = 2, // Time from companion server NTP
    GPS = 3            // GPS time (highest priority - most accurate)
};

class TimeHelper {

public:
    /**
     * Set time with source priority.
     * Only accepts time from equal or higher priority sources.
     *
     * @param epoch_secs Unix timestamp
     * @param source The source of this time update
     * @return true if time was set, false if rejected due to lower priority
     */
    bool setTime(time_t epoch_secs, TimeSource source);

    /**
     * Set time without source (defaults to MANUAL priority).
     * For backwards compatibility.
     */
    void setTime(time_t epoch_secs);

    void setPosixTimezone(const char* timezone_str);

    /**
     * Get the current time source.
     */
    TimeSource getTimeSource() const { return _currentSource; }

    /**
     * Get the name of a time source for display.
     */
    static const char* sourceToString(TimeSource source);

private:
    TimeSource _currentSource = TimeSource::NONE;

};