#ifdef RET_PLATFORM_EMU

#include "EmulatorKeyboard.h"
#include "lvgl.h"
#include <cstdio>

// External function to update last action time
extern void update_last_action_time();

// Display dimensions for coordinate scaling (must match EmulatorScreen)
#define EMU_DISP_HOR_RES 240
#define EMU_DISP_VER_RES 320
#define EMU_ZOOM_FACTOR 2

// Static state for LVGL keyboard callback
static uint32_t s_lastKey = 0;
static bool s_keyPressed = false;
static lv_indev_state_t s_keyState = LV_INDEV_STATE_RELEASED;

// Static state for LVGL mouse/touch callback
static int16_t s_mouseX = 0;
static int16_t s_mouseY = 0;
static lv_indev_state_t s_mouseState = LV_INDEV_STATE_RELEASED;
static lv_indev_t* s_mouseIndev = nullptr;

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

// Process all SDL events and update state for both keyboard and mouse
static void process_sdl_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN: {
                uint32_t key = sdl_key_to_lvgl(event.key.keysym.sym);
                if (key != 0) {
                    s_lastKey = key;
                    s_keyState = LV_INDEV_STATE_PRESSED;
                    s_keyPressed = true;
                    update_last_action_time();
                }
                break;
            }
            case SDL_KEYUP:
                s_keyState = LV_INDEV_STATE_RELEASED;
                break;

            case SDL_MOUSEMOTION:
                // Scale mouse coordinates from window to display coordinates
                s_mouseX = event.motion.x / EMU_ZOOM_FACTOR;
                s_mouseY = event.motion.y / EMU_ZOOM_FACTOR;
                // Clamp to display bounds
                if (s_mouseX < 0) s_mouseX = 0;
                if (s_mouseX >= EMU_DISP_HOR_RES) s_mouseX = EMU_DISP_HOR_RES - 1;
                if (s_mouseY < 0) s_mouseY = 0;
                if (s_mouseY >= EMU_DISP_VER_RES) s_mouseY = EMU_DISP_VER_RES - 1;
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    s_mouseX = event.button.x / EMU_ZOOM_FACTOR;
                    s_mouseY = event.button.y / EMU_ZOOM_FACTOR;
                    s_mouseState = LV_INDEV_STATE_PRESSED;
                    update_last_action_time();
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    s_mouseState = LV_INDEV_STATE_RELEASED;
                }
                break;

            case SDL_QUIT:
                printf("[EmulatorKeyboard] Quit requested\n");
                exit(0);
                break;

            default:
                break;
        }
    }
}

// LVGL keyboard read callback
static void keyboard_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;

    // Process all pending SDL events
    process_sdl_events();

    data->key = s_lastKey;
    data->state = s_keyState;
}

// LVGL mouse/touch read callback
static void mouse_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;

    // Events are processed in keyboard callback, just report state here
    data->point.x = s_mouseX;
    data->point.y = s_mouseY;
    data->state = s_mouseState;
}

void EmulatorKeyboard::initKeyboard() {
    printf("[EmulatorKeyboard] Initialized\n");
}

void EmulatorKeyboard::initLvgl() {
    printf("[EmulatorKeyboard] Registering LVGL input devices\n");

    // Register keyboard input device
    static lv_indev_drv_t kbd_drv;
    lv_indev_drv_init(&kbd_drv);
    kbd_drv.type = LV_INDEV_TYPE_KEYPAD;
    kbd_drv.read_cb = keyboard_read_cb;

    _indev = lv_indev_drv_register(&kbd_drv);

    if (_indev) {
        printf("[EmulatorKeyboard] LVGL keyboard registered successfully\n");
    } else {
        printf("[EmulatorKeyboard] Failed to register LVGL keyboard\n");
    }

    // Register mouse/touch input device (pointer type)
    static lv_indev_drv_t mouse_drv;
    lv_indev_drv_init(&mouse_drv);
    mouse_drv.type = LV_INDEV_TYPE_POINTER;
    mouse_drv.read_cb = mouse_read_cb;

    s_mouseIndev = lv_indev_drv_register(&mouse_drv);

    if (s_mouseIndev) {
        printf("[EmulatorKeyboard] LVGL mouse/touch registered successfully\n");
        // Set a cursor for visual feedback (optional, creates a small dot)
        // lv_obj_t* cursor = lv_obj_create(lv_scr_act());
        // lv_obj_set_size(cursor, 10, 10);
        // lv_indev_set_cursor(s_mouseIndev, cursor);
    } else {
        printf("[EmulatorKeyboard] Failed to register LVGL mouse/touch\n");
    }
}

void EmulatorKeyboard::processEvents() {
    // Events are processed in the LVGL callback
    // This method can be used for non-LVGL key processing if needed
}

#endif
