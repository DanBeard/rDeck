#pragma once
#include <stdint.h>
#include <TinyGPS++.h>

class BaseGPS {


    public:

        // init the hardware. Called once on boot
        virtual void initGPS() = 0;
        // called in loop()
        virtual void tick() = 0;

        // just returning this for now since I dont know what abstraction would be better
        // or what other driver's capabilities are. Might have to refactor later *Shrug*
        TinyGPSPlus* GPS = nullptr;



};