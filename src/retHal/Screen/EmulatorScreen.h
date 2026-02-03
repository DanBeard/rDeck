#pragma once
#ifdef RET_PLATFORM_EMU
#include "BaseScreen.h"
#include <SDL2/SDL.h>

class EmulatorScreen : public BaseScreen {
public:
    // called once on boot
    virtual void initScreen() override;

    // Draw the startup screen. Usually NOT with lvgl but called after init()
    virtual void drawStartupScreen() override;

    // init lvgl, called after initScreen and startup screen draw
    virtual void initLvgl() override;

    // full refresh for e-ink type displays that differentiate
    virtual void fullRefresh() override;

    // Force a full e-ink refresh cycle (no-op for emulator)
    virtual void forceFullRefresh(uint8_t color = 0xFF) override;

    // SDL event processing - call in main loop
    void processEvents();

    // Check if quit was requested
    bool shouldQuit() const { return _quit; }

private:
    SDL_Window* _window = nullptr;
    SDL_Renderer* _renderer = nullptr;
    SDL_Texture* _texture = nullptr;
    uint32_t* _framebuffer = nullptr;
    bool _quit = false;
};

#endif
