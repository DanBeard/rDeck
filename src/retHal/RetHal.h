#pragma once

#include "Screen/BaseScreen.h"
#include "Keyboard/BaseKeyboard.h"
#include "FS.h" // ARduino already has a good enough abstraction. Stick to that.
#include "Battery/BaseBattery.h"
#include "GPS/BaseGPS.h"
#include "Lora/BaseLora.h"


typedef void (*RetTask)(void *);

struct RetHal {

    // required peropherals. The system assumes these are here even if they are stubbed out
    BaseScreen*        screen;
    FS*                fs; // filesystem
    BaseKeyboard*     keyboard;

    // optional. set to nullptr if not supported
    BaseBattery* battery;
    BaseGPS*     gps;
    BaseLora* lora;

    // register the main ui task that handles the active app
    // and UI drawing
    void (*register_ui_task)(RetTask task);

    // register the services task that handles running services
    // this should be higher priority than the ui task if possible

    void (*register_service_task)(RetTask task);

    // time of last action in hardware. used for sleep calculations
    uint64_t (*time_of_last_action)();
    
    void (*light_sleep)();


};
