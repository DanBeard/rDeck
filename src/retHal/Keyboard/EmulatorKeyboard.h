#pragma once
#ifdef RET_PLATFORM_EMU

#include "BaseKeyboard.h"
#include <SDL2/SDL.h>

class EmulatorKeyboard : public BaseKeyboard {
public:
    // init the hardware. Called once on boot
    virtual void initKeyboard() override;

    // init lvgl, called after initScreen and startup screen draw
    virtual void initLvgl() override;

    // Process SDL keyboard events - called from main loop
    void processEvents();

    // Get last key pressed (for non-LVGL use)
    uint32_t getLastKey() const { return _lastKey; }
    bool hasKeyPressed() const { return _keyPressed; }
    void clearKeyPressed() { _keyPressed = false; }

private:
    uint32_t _lastKey = 0;
    bool _keyPressed = false;
    bool _keyDown = false;
};

#endif
