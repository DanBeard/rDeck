#pragma once
#include "BaseBattery.h"

class TDeckProBattery : public BaseBattery{


    public:

        // init the hardware. Called once on boot
        virtual void initBattery();
        virtual uint8_t percent(); //0 -> 100
        virtual bool charging();


};