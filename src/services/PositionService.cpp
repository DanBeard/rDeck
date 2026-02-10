#include "PositionService.h"
#include "lvgl.h"
#include "retOS/retosUtils/TimeHelper.h"
#include "retOS/Events.h"

void PositionService::start(RetOS* retos){
    _gps = retos->hal().gps;
    updateIcon(false);

    // set to running
    _status = RUNNING;
}

void PositionService::tick(const unsigned long tMillis) {
    _gps->tick();
    if (!_gps->GPS) return;

    bool newIsValid = _gps->GPS->location.isValid();
    if(newIsValid != isValid){
        updateIcon(newIsValid);
        isValid = newIsValid;
        this->actionHappened(); // only update our action timer if we had a status change
    }

    // Sync time from GPS when we have a valid time fix
    if (_gps->GPS->time.isValid() && _gps->GPS->date.isValid()) {
        // Only attempt sync once per interval to avoid spam
        if (tMillis - _lastTimeSync > GPS_TIME_SYNC_INTERVAL || tMillis < _lastTimeSync) {
            uint16_t year = _gps->GPS->date.year();
            uint8_t month = _gps->GPS->date.month();
            uint8_t day = _gps->GPS->date.day();

            // Sanity check: TinyGPS++ isValid() lies - it returns true for default/garbage data
            // Real GPS data must have: year 2020-2100, month 1-12, day 1-31
            bool dateIsValid = (year >= 2020 && year <= 2100 &&
                               month >= 1 && month <= 12 &&
                               day >= 1 && day <= 31);

            if (!dateIsValid) {
                // Only log invalid date once per minute to avoid spam
                static unsigned long lastInvalidLog = 0;
                if (tMillis - lastInvalidLog > 60000 || tMillis < lastInvalidLog) {
                    Serial.printf("[GPS] Waiting for valid fix (got %04d-%02d-%02d)\n", year, month, day);
                    lastInvalidLog = tMillis;
                }
                _lastTimeSync = tMillis;  // Still update to throttle checks
                return;
            }

            struct tm gpsTime;
            gpsTime.tm_year = year - 1900;
            gpsTime.tm_mon = month - 1;
            gpsTime.tm_mday = day;
            gpsTime.tm_hour = _gps->GPS->time.hour();
            gpsTime.tm_min = _gps->GPS->time.minute();
            gpsTime.tm_sec = _gps->GPS->time.second();
            gpsTime.tm_isdst = 0;

            time_t epoch = mktime(&gpsTime);
            if (epoch > 0) {
                Serial.printf("[GPS] Valid time: %04d-%02d-%02d %02d:%02d:%02d -> epoch %lu\n",
                              year, month, day,
                              gpsTime.tm_hour, gpsTime.tm_min, gpsTime.tm_sec,
                              (unsigned long)epoch);
                _retos->time.setTime(epoch, TimeSource::GPS);
                _lastTimeSync = tMillis;
            }
        }
    }

    // Publish location events when GPS has a valid fix
    if (_gps->GPS->location.isValid()) {
        if (tMillis - _lastLocationPublish > LOCATION_PUBLISH_INTERVAL || tMillis < _lastLocationPublish) {
            Event e;
            e.src = this;
            e.type = EventType::LOCATION_CHANGE;
            e.args[0] = (uint32_t)(int32_t)(_gps->GPS->location.lat() * 1e7);
            e.args[1] = (uint32_t)(int32_t)(_gps->GPS->location.lng() * 1e7);
            e.args[2] = (uint32_t)(_gps->GPS->course.deg() * 100);  // heading with 0.01 resolution
            e.args[3] = _gps->GPS->course.isValid() ? 1 : 0;
            publishEvent(e);
            _lastLocationPublish = tMillis;
        }
    }
}

bool PositionService::hasValidPosition() const {
    return _gps && _gps->GPS && _gps->GPS->location.isValid();
}

double PositionService::getLatitude() const {
    return hasValidPosition() ? _gps->GPS->location.lat() : 0.0;
}

double PositionService::getLongitude() const {
    return hasValidPosition() ? _gps->GPS->location.lng() : 0.0;
}

bool PositionService::hasValidHeading() const {
    return _gps && _gps->GPS && _gps->GPS->course.isValid();
}

float PositionService::getHeading() const {
    return hasValidHeading() ? (float)_gps->GPS->course.deg() : 0.0f;
}

void PositionService::updateIcon(bool status){
    ServiceIcon iconInfo = {
        .serviceID = this->_id,
        .icon = status ? LV_SYMBOL_GPS : "G?",
        .opacity = status ? LV_OPA_100 : LV_OPA_100,
    };
    _retos->ui()->setServiceIcon(iconInfo);
}
