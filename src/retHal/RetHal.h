#pragma once

#include "Screen/BaseScreen.h"
#include "Keyboard/BaseKeyboard.h"
#include "FS.h" // ARduino already has a good enough abstraction. Stick to that.
#include "Battery/BaseBattery.h"

struct RetHal {

    // required peropherals. The system assumes these are here even if they are stubbed out
    BaseScreen*        screen;
    FS*                fs; // filesystem
    BaseKeyboard*     keyboard;

    // optional. set to nullptr if not supported
    BaseBattery* battery;

};
