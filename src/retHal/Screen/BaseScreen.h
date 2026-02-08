#pragma once


class BaseScreen {


    public:

        // init the hardware. Called once on boot
        virtual void initScreen() = 0;

        // Draw the startup screen. Usually NOT with lvgl but called after init()
        virtual void drawStartupScreen() = 0;

        // init lvgl, called after initScreen and startup screen draw
        virtual void initLvgl() = 0;

        // full refresh for e-ink type displays that differentiate
        virtual void fullRefresh() = 0;

        // Force a full e-ink refresh cycle to clear ghosting
        // Bypasses LVGL and calls GxEPD2 clearScreen() directly
        // color: 0x00 = black, 0xFF = white
        virtual void forceFullRefresh(uint8_t color = 0xFF) = 0;

};