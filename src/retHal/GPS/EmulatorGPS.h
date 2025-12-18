#pragma once
#ifdef RET_PLATFORM_EMU

#include "BaseGPS.h"

/**
 * EmulatorGPS - Stub GPS implementation for emulator.
 * GPS is nullptr - services should handle null GPS gracefully.
 */
class EmulatorGPS : public BaseGPS {
public:
    // init the hardware. Called once on boot
    virtual void initGPS() override;

    // called in loop()
    virtual void tick() override;
};

#endif
