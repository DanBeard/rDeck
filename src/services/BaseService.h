#pragma once

#include <ArduinoJson.h>
#include "../retOS/Events.h"
#include "../retOS/retOS.h"


using namespace std;

enum ServiceStatus {
    STARTING = 0, // initializing. This WILL block startup since services should use async methods
    RUNNING, // running, all good dude
    STOPPED, // stopped but like, not for an error man.
    ERROR  // oops!

};

// Unlike apps services start once and then keep going, even in the background
// As such, MUCH more attention needs to be paid to timing, code size and memory allocation
// avoid dynamic memory when possible, and always clean up after yourself.
// Keep polling to a minimum. Try being event drvien using the events

#define SERVICE_ID_FLAG 0x80
class BaseService {

protected:
    BaseService(const uint8_t id);
    // As a convention to avoid conflict with apps, Services start at 0x80 aka 128 aka high bit
    const uint8_t _id; // ID MUST be unique among all installed services. This makes it easier to quickly identify apps
    RetOS* _retos; // pointer to the os object
    ServiceStatus _status = STARTING;

public:
    virtual void start(RetOS* retos); // called after construction once the OS is ready to launch services
    virtual void loop() = 0; // called periodically by OS so you can do work. TIme varies by sleep and power level.
    virtual EventStatus onEvent(Event& event) = 0;

    const uint8_t id() const;
    const ServiceStatus status() const;
};