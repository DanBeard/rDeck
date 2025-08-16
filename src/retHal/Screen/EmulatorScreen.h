#pragma once
#ifdef RET_PLATFORM_EMU
#include "BaseScreen.h"

class TDeckProScreen : public BaseScreen {

public:
        // called once on boot
        virtual void initScreen();

        // Draw the startup screen. Usually NOT with lvgl but called after init()
        virtual void drawStartupScreen();

        // init lvgl, called after initScreen and startup screen draw
        virtual void initLvgl();

        // full refresh for e-ink type displays that differentiate
        virtual void fullRefresh(); 


};

#endif