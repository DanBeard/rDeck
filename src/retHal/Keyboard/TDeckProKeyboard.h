#pragma once
#include "BaseKeyboard.h"

class TDeckProKeyboard : public BaseKeyboard{


    public:

        // init the hardware. Called once on boot
        virtual void initKeyboard();

        // init lvgl, called after initScreen and startup screen draw
        virtual void initLvgl();


};