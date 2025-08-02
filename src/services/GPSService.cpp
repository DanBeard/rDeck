#include "GPSService.h"
#include "lvgl.h"

void GPSService::start(RetOS* retos){
    _gps = retos->hal().gps;
    updateIcon(false);

    // set to running
    _status = RUNNING;
}
void GPSService::loop() {
    _gps->tick();
    bool newIsValid = _gps->GPS->location.isValid();
    if(newIsValid != isValid){
        updateIcon(newIsValid);
        isValid = newIsValid;
    }
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
EventStatus GPSService::onEvent(Event& event){
     return IGNORED;
}