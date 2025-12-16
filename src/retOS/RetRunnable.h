#pragma once

// I hate that I have to do this, for things like generic service lookup by template type, a small baseclass for
// borth services and apps that is just the ID part that retOS cares about is needed

#include "string.h"
#include <Arduino.h>

class RetRunnable {

protected:
    const uint8_t _id; // ID MUST be unique among all installed services. This makes it easier to quickly identify apps

public:
    
    const uint8_t id() const { return _id; };

    RetRunnable(uint8_t id) : _id(id) {};

    virtual void tick(const unsigned long tickMillis) = 0; // called periodically by OS so you can do work. TIme varies by sleep and power level.

};