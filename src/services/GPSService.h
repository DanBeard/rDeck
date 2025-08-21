#pragma once
#include "BaseService.h"
#include "retHal/GPS/BaseGPS.h"

class GPSService: public BaseService {
    
    using BaseService::BaseService;

public:

    virtual void start(RetOS* retos) override; // called after construction once the OS is ready to launch services
    virtual void tick(const time_t tmillis) override; // called periodically by OS so you can do work. TIme varies by sleep and power level.

    bool isValid = false;

protected:
    BaseGPS* _gps = nullptr;
    void updateIcon(bool status);

};