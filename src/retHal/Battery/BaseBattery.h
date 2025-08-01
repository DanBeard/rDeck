#pragma once
#include <stdint.h>

class BaseBattery {


    public:

        // init the hardware. Called once on boot
        virtual void initBattery() = 0;

        virtual uint8_t percent()= 0; //0 -> 100
        virtual bool charging() = 0;


};