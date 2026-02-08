#include "GPSService.h"
#include "lvgl.h"
#include "retOS/retosUtils/TimeHelper.h"

void GPSService::start(RetOS* retos){
    _gps = retos->hal().gps;
    updateIcon(false);

    // set to running
    _status = RUNNING;
}

void GPSService::tick(const unsigned long tMillis) {
    _gps->tick();
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
}

void GPSService::updateIcon(bool status){
    ServiceIcon iconInfo = {
        .serviceID = this->_id,
        .icon = status ? LV_SYMBOL_GPS : "G?",
        .opacity = status ? LV_OPA_100 : LV_OPA_100,
    };
    _retos->ui()->setServiceIcon(iconInfo);
}
