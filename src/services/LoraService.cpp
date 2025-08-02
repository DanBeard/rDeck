#include "LoraService.h"
#include "lvgl.h"

void LoraService::start(RetOS* retos){
    updateIcon(false);

    // set to running
    _status = RUNNING;
}
void LoraService::loop() {
    
}

void LoraService::updateIcon(bool status){
    ServiceIcon iconInfo = {
        .serviceID = this->_id,
        .icon = status ? LV_SYMBOL_WIFI : "L?",
        .opacity = status ? LV_OPA_100 : LV_OPA_100,
    };
    _retos->ui()->setServiceIcon(iconInfo);
}
EventStatus LoraService::onEvent(Event& event){
     return IGNORED;
}