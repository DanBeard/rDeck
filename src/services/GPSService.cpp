#include "GPSService.h"
#include "lvgl.h"

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

    // TODO sync up RTC with the GPS time and keep track so we know the real time
    // Won't really be useful until we get a settings app with timezones and stuff 

    // TODO once event system works. Send out events when things happen? like when you're somewhere?
}

void GPSService::updateIcon(bool status){
    ServiceIcon iconInfo = {
        .serviceID = this->_id,
        .icon = status ? LV_SYMBOL_GPS : "G?",
        .opacity = status ? LV_OPA_100 : LV_OPA_100,
    };
    _retos->ui()->setServiceIcon(iconInfo);
}
