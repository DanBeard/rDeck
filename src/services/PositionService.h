#pragma once
#include "BaseService.h"
#include "retHal/GPS/BaseGPS.h"

class PositionService: public BaseService {

    using BaseService::BaseService;

public:
    // Time sync interval - sync GPS time every 60 seconds
    static const unsigned long GPS_TIME_SYNC_INTERVAL = 60 * 1000;

    virtual void start(RetOS* retos) override; // called after construction once the OS is ready to launch services
    virtual void tick(const unsigned long tmillis) override; // called periodically by OS so you can do work. TIme varies by sleep and power level.

    bool isValid = false;

    // Position query API
    bool hasValidPosition() const;
    double getLatitude() const;
    double getLongitude() const;
    bool hasValidHeading() const;
    float getHeading() const;

protected:
    BaseGPS* _gps = nullptr;
    unsigned long _lastTimeSync = 0;
    unsigned long _lastLocationPublish = 0;
    static const unsigned long LOCATION_PUBLISH_INTERVAL = 2000;  // Publish GPS every 2s
    void updateIcon(bool status);

};
