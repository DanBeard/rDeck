#pragma once

#include "BaseScreen.h"

class TDeckProScreen : public BaseScreen {

public:
        // called once on boot
        virtual void initScreen() override;

        // Draw the startup screen. Usually NOT with lvgl but called after init()
        virtual void drawStartupScreen() override;

        // init lvgl, called after initScreen and startup screen draw
        virtual void initLvgl() override;

        // full refresh for e-ink type displays that differentiate
        virtual void fullRefresh() override;

        // Force a full e-ink refresh cycle to clear ghosting
        virtual void forceFullRefresh(uint8_t color = 0xFF) override;

};