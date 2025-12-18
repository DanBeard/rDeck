#ifdef RET_PLATFORM_EMU

#include "EmulatorScreen.h"
#include "lvgl.h"
#include <cstdio>
#include <cstring>

// Display dimensions (match e-ink display)
#define DISP_HOR_RES 320
#define DISP_VER_RES 240
#define ZOOM_FACTOR 2  // 2x scaling for better visibility

// Static pointers for LVGL callbacks
static SDL_Window* s_window = nullptr;
static SDL_Renderer* s_renderer = nullptr;
static SDL_Texture* s_texture = nullptr;
static uint32_t* s_framebuffer = nullptr;

// LVGL draw buffers
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t* s_buf1 = nullptr;
static lv_color_t* s_buf2 = nullptr;

// LVGL flush callback - converts 1bpp to ARGB8888 for SDL display
static void sdl_flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    if (!s_framebuffer || !s_texture || !s_renderer) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // Copy pixels from LVGL buffer to SDL framebuffer
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            int idx = y * DISP_HOR_RES + x;
            // LVGL 1bpp: white = 1, black = 0
            // Convert to ARGB8888 for SDL
            #if LV_COLOR_DEPTH == 1
            uint8_t gray = color_p->full ? 0xFF : 0x00;
            #else
            // For other color depths, convert to grayscale
            uint8_t gray = lv_color_brightness(*color_p);
            #endif
            s_framebuffer[idx] = (0xFF << 24) | (gray << 16) | (gray << 8) | gray;
            color_p++;
        }
    }

    // Update texture and render
    SDL_UpdateTexture(s_texture, nullptr, s_framebuffer, DISP_HOR_RES * sizeof(uint32_t));
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, nullptr, nullptr);
    SDL_RenderPresent(s_renderer);

    lv_disp_flush_ready(disp_drv);
}

void EmulatorScreen::initScreen() {
    printf("[EmulatorScreen] Initializing SDL2...\n");

    // Workaround for SDL2 crash on some systems
    #ifndef WIN32
    setenv("DBUS_FATAL_WARNINGS", "0", 1);
    #endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        printf("[EmulatorScreen] SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    _window = SDL_CreateWindow(
        "rDeck Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DISP_HOR_RES * ZOOM_FACTOR, DISP_VER_RES * ZOOM_FACTOR,
        SDL_WINDOW_SHOWN
    );

    if (!_window) {
        printf("[EmulatorScreen] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }

    _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!_renderer) {
        printf("[EmulatorScreen] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return;
    }

    _texture = SDL_CreateTexture(
        _renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        DISP_HOR_RES, DISP_VER_RES
    );

    if (!_texture) {
        printf("[EmulatorScreen] SDL_CreateTexture failed: %s\n", SDL_GetError());
        return;
    }

    _framebuffer = new uint32_t[DISP_HOR_RES * DISP_VER_RES];

    // Store in static pointers for callbacks
    s_window = _window;
    s_renderer = _renderer;
    s_texture = _texture;
    s_framebuffer = _framebuffer;

    printf("[EmulatorScreen] SDL2 initialized successfully (%dx%d, zoom %d)\n",
           DISP_HOR_RES, DISP_VER_RES, ZOOM_FACTOR);
}

void EmulatorScreen::drawStartupScreen() {
    printf("[EmulatorScreen] Drawing startup screen...\n");

    if (!_framebuffer || !_texture || !_renderer) return;

    // Clear to white (e-ink default background)
    for (int i = 0; i < DISP_HOR_RES * DISP_VER_RES; i++) {
        _framebuffer[i] = 0xFFFFFFFF;  // White (ARGB)
    }

    SDL_UpdateTexture(_texture, nullptr, _framebuffer, DISP_HOR_RES * sizeof(uint32_t));
    SDL_RenderClear(_renderer);
    SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
    SDL_RenderPresent(_renderer);
}

void EmulatorScreen::initLvgl() {
    printf("[EmulatorScreen] Initializing LVGL...\n");

    lv_init();

    // Allocate draw buffers
    s_buf1 = new lv_color_t[DISP_HOR_RES * DISP_VER_RES];
    s_buf2 = new lv_color_t[DISP_HOR_RES * DISP_VER_RES];

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, DISP_HOR_RES * DISP_VER_RES);

    // Configure display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    disp_drv.flush_cb = sdl_flush_cb;
    disp_drv.draw_buf = &s_draw_buf;
    disp_drv.full_refresh = 1;  // Full refresh like e-ink display

    lv_disp_drv_register(&disp_drv);

    printf("[EmulatorScreen] LVGL initialized\n");
}

void EmulatorScreen::fullRefresh() {
    if (!_renderer || !_texture) return;

    SDL_RenderClear(_renderer);
    SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
    SDL_RenderPresent(_renderer);
}

void EmulatorScreen::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            _quit = true;
        }
        // Keyboard events are handled by EmulatorKeyboard
    }
}

#endif
