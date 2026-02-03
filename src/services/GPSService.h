#pragma once
#include "BaseService.h"
#include "retHal/GPS/BaseGPS.h"

class GPSService: public BaseService {

    using BaseService::BaseService;

public:
    // Time sync interval - sync GPS time every 60 seconds
    static const unsigned long GPS_TIME_SYNC_INTERVAL = 60 * 1000;

    virtual void start(RetOS* retos) override; // called after construction once the OS is ready to launch services
    virtual void tick(const unsigned long tmillis) override; // called periodically by OS so you can do work. TIme varies by sleep and power level.

    bool isValid = false;

protected:
    BaseGPS* _gps = nullptr;
    unsigned long _lastTimeSync = 0;
    void updateIcon(bool status);

};