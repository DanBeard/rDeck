 #include "BaseService.h"

 BaseService::BaseService(const uint8_t id) :
 RetRunnable(id) {

 }

 void BaseService::startService(RetOS* retos){
    _retos = retos;
    this->start(retos);
 }

const ServiceStatus BaseService::status() const{
        return _status;
}


EventStatus BaseService::onEvent(const Event& event) {
    return IGNORED;
}

