#pragma once
#include "lvgl.h"

class BaseKeyboard {


    public:

        // init the hardware. Called once on boot
        virtual void initKeyboard() = 0;

        // init lvgl, called after initScreen and startup screen draw
        virtual void initLvgl() = 0;

        lv_indev_t* indev() {return _indev;};
        
    protected:
        lv_indev_t* _indev;


};