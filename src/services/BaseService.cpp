 #include "BaseService.h"

 BaseService::BaseService(const uint8_t id) :
  _id(id) {

 }


 void BaseService::startService(RetOS* retos){
    _retos = retos;
    this->start(retos);
 }

const uint8_t BaseService::id() const {
    return _id;
}
const ServiceStatus BaseService::status() const{
        return _status;
}