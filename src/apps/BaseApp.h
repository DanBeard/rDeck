#pragma once
#include "../retOS/RetRunnable.h"
#include <Arduino.h>
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
class BaseApp : public RetRunnable {
public:
    explicit BaseApp(const char *name, const uint8_t _id);

protected:
    
    char _name[RETOS_MAX_APP_NAME_SIZE + 1];
    // As a convention to avoid conflict with services, Apps start at 0x00 and are less than 0x80 aka < 128 aka low bit

    RetOS* _retos; // pointer to the os object
    lv_obj_t* screen; // the LVGL screen for this app to draw on. If you stay inside these bounds you don't mess up OS widgets
    virtual void start(RetOS* retos) = 0; // called after construction once the UI is ready to be drawn on

    uint64_t _lastActionTime = 0;
    bool _keep_awake = false; //keep dek awake while this app is active. REALLY bad for battery life!
    void actionHappened(){_lastActionTime = millis();}

public:
    void startApp(RetOS* retos);
    
    virtual void stop() = 0; // called when the app is closed, before destruction
    uint64_t timeOfLastAction() { return _lastActionTime;}// returns the time of the last action in millis(). Used for sleep calculations
    bool keepAwake() const {return _keep_awake;}
    virtual EventStatus onEvent(const Event& event);

    // special back button action, if the app itsself wants to control what happens. 
    // returning an  empty function<> means do the default of closing the app entirely
    virtual const function<void()> customBackButtonAction()   { return function<void()>(); }


};