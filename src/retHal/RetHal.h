#pragma once

#include "Screen/BaseScreen.h"
#include "Keyboard/BaseKeyboard.h"
#include "FS.h" // ARduino already has a good enough abstraction. Stick to that.

struct RetHal {

    BaseScreen*        screen;
    FS*                fs; // filesystem
    BaseKeyboard*     keyboard;

};
