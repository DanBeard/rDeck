#pragma once
#ifdef RET_PLATFORM_EMU

#include "BaseBattery.h"

/**
 * EmulatorBattery - Stub battery implementation for emulator.
 * Always reports 100% battery, always charging.
 */
class EmulatorBattery : public BaseBattery {
public:
    // init the hardware. Called once on boot
    virtual void initBattery() override {
        // No-op for emulator
    }

    // Returns battery percentage (0-100)
    virtual uint8_t percent() override {
        return 100;  // Always full
    }

    // Returns true if charging
    virtual bool charging() override {
        return true;  // Always "plugged in"
    }
};

#endif
