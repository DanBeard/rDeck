#pragma once

#include <ArduinoJson.h>
#include "../retOS/retOS.h"
#include "../retOS/Events.h"

using namespace std;

#define RETOS_MAX_APP_NAME_SIZE 15

/*
* The abstract base class for all apps on rdeck. 
* There is only ever 1 active app at a time. (may change to a stack later but doubtful)
* App lifecycle is only while it has control of the screen.
* The retOS will destroy the app when it is exited by the user
* Avoid dynamic memory if possible and clean up after yourself!
*/
class BaseApp {
public:
    explicit BaseApp(const char *name, const uint8_t _id);

protected:
    
    
    char _name[RETOS_MAX_APP_NAME_SIZE + 1];
    // As a convention to avoid conflict with services, Apps start at 0x00 and are less than 0x80 aka < 128 aka low bit
    const uint8_t _id; // ID MUST be unique among all installed apps. This makes it easier to quickly identify apps
    RetOS* _retos; // pointer to the os object
    DynamicJsonDocument _args; // args used when opening the app

public:
    virtual void start(JsonDocument& args, RetOS* retos); // called after construction once the UI is ready to be drawn on
    virtual void loop() = 0; // called periodically by OS so you can do work. TIme varies by sleep and power level.
    virtual void stop() = 0; // called when the app is closed, before destruction

    virtual EventStatus onEvent(Event& event) = 0;

    const uint8_t id() const;

};