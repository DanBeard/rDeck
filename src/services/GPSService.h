#pragma once
#include "BaseService.h"
#include "retHal/GPS/BaseGPS.h"

class GPSService: public BaseService {
    
    using BaseService::BaseService;

public:

    virtual void start(RetOS* retos); // called after construction once the OS is ready to launch services
    virtual void tick(); // called periodically by OS so you can do work. TIme varies by sleep and power level.
    virtual EventStatus onEvent(Event& event);

    bool isValid = false;

protected:
    BaseGPS* _gps = nullptr;
    void updateIcon(bool status);

};