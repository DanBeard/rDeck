#ifdef RET_PLATFORM_EMU

#include "EmulatorScreen.h"
#include "lvgl.h"
#include <Arduino.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#include <SDL2/SDL.h>
#include "drivers/sdl/lv_sdl_mouse.h"
#include "drivers/sdl/lv_sdl_mousewheel.h"
#include "drivers/sdl/lv_sdl_keyboard.h"

static lv_display_t *lvDisplay;
static lv_indev_t *lvMouse;
static lv_indev_t *lvMouseWheel;
static lv_indev_t *lvKeyboard;


/*virtual */ void TDeckProScreen::initScreen() {
   
}

/* virtual */ void TDeckProScreen::drawStartupScreen() {
    Serial.println("Drawing startup screen.....");
}

void hal_setup(void)
{
    // Workaround for sdl2 `-m32` crash
    // https://bugs.launchpad.net/ubuntu/+source/libsdl2/+bug/1775067/comments/7
    #ifndef WIN32
        setenv("DBUS_FATAL_WARNINGS", "0", 1);
    #endif

    #if LV_USE_LOG != 0
    lv_log_register_print_cb(lv_log_print_g_cb);
    #endif

    /* Add a display
     * Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/


    lvDisplay = lv_sdl_window_create(SDL_HOR_RES, SDL_VER_RES);
    lvMouse = lv_sdl_mouse_create();
    lvMouseWheel = lv_sdl_mousewheel_create();
    lvKeyboard = lv_sdl_keyboard_create();
}

void hal_loop(void)
{
    Uint32 lastTick = SDL_GetTicks();
    while(1) {
        SDL_Delay(5);
        Uint32 current = SDL_GetTicks();
        lv_tick_inc(current - lastTick); // Update the tick timer. Tick is new for LVGL 9
        lastTick = current;
        lv_timer_handler(); // Update the UI-
    }
}

/* virtual */ void TDeckProScreen::initLvgl() {
    lv_init();

    hal_setup();
}


/* virtual */ void TDeckProScreen::fullRefresh() {

    // static lv_coord_t last_x = 0;
    // static lv_coord_t last_y = 0;

    // uint8_t touched = tDeckProTouch.getPoint(&last_x, &last_y, 1);
    // Serial.printf("[t=%u x=%u y=%u]    ", touched, last_x, last_y);
}

#endif