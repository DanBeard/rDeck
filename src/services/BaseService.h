#pragma once

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
class BaseService {

protected:
    const uint8_t _id; // ID MUST be unique among all installed services. This makes it easier to quickly identify apps
    RetOS* _retos; // pointer to the os object
    ServiceStatus _status = STARTING;

    uint64_t _lastActionTime = 0;
    inline void publishEvent(const Event& e) {_retos->publishEvent(e);};


public:
    BaseService(const uint8_t id);
    virtual void start(RetOS* retos)=0; // called after construction once the OS is ready to launch services
    virtual void tick() = 0; // called periodically by OS so you can do work. TIme varies by sleep and power level.

    virtual EventStatus onEvent(const Event& event);

    const uint8_t id() const;
    const ServiceStatus status() const;
    void startService(RetOS* retos);

    void actionHappened(){_lastActionTime = millis();} // public so helpers can access it
    uint64_t timeOfLastAction() const { return _lastActionTime;}// returns the time of the last action in millis(). Used for sleep calculations
    
};