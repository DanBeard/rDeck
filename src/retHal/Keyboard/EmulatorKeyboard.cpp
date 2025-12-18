#ifdef RET_PLATFORM_EMU

#include "EmulatorKeyboard.h"
#include "lvgl.h"
#include <cstdio>

// External function to update last action time
extern void update_last_action_time();

// Static state for LVGL callback
static uint32_t s_lastKey = 0;
static bool s_keyPressed = false;
static lv_indev_state_t s_keyState = LV_INDEV_STATE_RELEASED;

// Map SDL key to LVGL key
static uint32_t sdl_key_to_lvgl(SDL_Keycode sdlKey) {
    switch (sdlKey) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return LV_KEY_ENTER;
        case SDLK_BACKSPACE:
            return LV_KEY_BACKSPACE;
        case SDLK_DELETE:
            return LV_KEY_DEL;
        case SDLK_ESCAPE:
            return LV_KEY_ESC;
        case SDLK_UP:
            return LV_KEY_UP;
        case SDLK_DOWN:
            return LV_KEY_DOWN;
        case SDLK_LEFT:
            return LV_KEY_LEFT;
        case SDLK_RIGHT:
            return LV_KEY_RIGHT;
        case SDLK_HOME:
            return LV_KEY_HOME;
        case SDLK_END:
            return LV_KEY_END;
        case SDLK_TAB:
            return LV_KEY_NEXT;
        default:
            // For regular characters, return ASCII value
            if (sdlKey >= SDLK_SPACE && sdlKey <= SDLK_z) {
                return (uint32_t)sdlKey;
            }
            return 0;
    }
}

// LVGL keyboard read callback
static void keyboard_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;

    // Process pending SDL events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN) {
            uint32_t key = sdl_key_to_lvgl(event.key.keysym.sym);
            if (key != 0) {
                s_lastKey = key;
                s_keyState = LV_INDEV_STATE_PRESSED;
                s_keyPressed = true;
                update_last_action_time();
            }
        } else if (event.type == SDL_KEYUP) {
            s_keyState = LV_INDEV_STATE_RELEASED;
        } else if (event.type == SDL_QUIT) {
            // Exit on window close
            printf("[EmulatorKeyboard] Quit requested\n");
            exit(0);
        }
    }

    data->key = s_lastKey;
    data->state = s_keyState;
}

void EmulatorKeyboard::initKeyboard() {
    printf("[EmulatorKeyboard] Initialized\n");
}

void EmulatorKeyboard::initLvgl() {
    printf("[EmulatorKeyboard] Registering LVGL keyboard input device\n");

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keyboard_read_cb;

    _indev = lv_indev_drv_register(&indev_drv);

    if (_indev) {
        printf("[EmulatorKeyboard] LVGL keyboard registered successfully\n");
    } else {
        printf("[EmulatorKeyboard] Failed to register LVGL keyboard\n");
    }
}

void EmulatorKeyboard::processEvents() {
    // Events are processed in the LVGL callback
    // This method can be used for non-LVGL key processing if needed
}

#endif
