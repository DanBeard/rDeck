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
        // Only sync once per minute to avoid excessive updates
        if (tMillis - _lastTimeSync > GPS_TIME_SYNC_INTERVAL || tMillis < _lastTimeSync) {
            struct tm gpsTime;
            gpsTime.tm_year = _gps->GPS->date.year() - 1900;
            gpsTime.tm_mon = _gps->GPS->date.month() - 1;
            gpsTime.tm_mday = _gps->GPS->date.day();
            gpsTime.tm_hour = _gps->GPS->time.hour();
            gpsTime.tm_min = _gps->GPS->time.minute();
            gpsTime.tm_sec = _gps->GPS->time.second();
            gpsTime.tm_isdst = 0;

            time_t epoch = mktime(&gpsTime);
            if (epoch > 0) {
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
