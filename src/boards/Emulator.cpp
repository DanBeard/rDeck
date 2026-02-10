#ifdef RET_PLATFORM_EMU

#include <thread>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <SDL2/SDL.h>

#include "Emulator.h"
#include "retHal/Screen/EmulatorScreen.h"
#include "retHal/Keyboard/EmulatorKeyboard.h"
#include "retHal/Battery/EmulatorBattery.h"
#include "retHal/GPS/EmulatorGPS.h"
#include "retHal/Lora/EmulatorLora.h"
#include "FS.h"

// External filesystem instance from ArduinoGlobals.cpp
extern FS SPIFFS;

// HAL component instances
static EmulatorScreen screen;
static EmulatorKeyboard kb;
static EmulatorBattery bat;
static EmulatorGPS gps;
static EmulatorLora lora;

// Thread handles
static std::thread* servicesThread = nullptr;
static std::atomic<bool> running{true};

// Time tracking
static std::atomic<uint64_t> time_of_last_action{0};

uint64_t time_of_last_action_getter() {
    return time_of_last_action.load();
}

void update_last_action_time() {
    time_of_last_action.store(SDL_GetTicks());
}

// UI task runs on main thread (required for SDL)
void register_ui_task(RetTask task) {
    printf("[Emulator] Starting UI task on main thread\n");
    // UI task is run directly - caller handles the loop
    task(nullptr);
}

// Services task runs on separate thread
void register_service_task(RetTask task) {
    printf("[Emulator] Starting services task on background thread\n");
    servicesThread = new std::thread([task]() {
        // Catch all exceptions to prevent std::terminate on the detached thread
        try {
            task(nullptr);
        } catch (...) {}
    });
    servicesThread->detach();
}

// Light sleep - just delay for emulator
void light_sleep() {
    printf("[Emulator] Light sleep (5 second delay)\n");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    printf("[Emulator] Woke from sleep\n");
}

// The HAL struct - note: using emuHal not tDeckProHal!
RetHal emuHal = {
    .screen = &screen,
    .fs = &SPIFFS,  // Use POSIX-based filesystem from ArduinoGlobals.cpp
    .keyboard = &kb,
    .battery = &bat,
    .gps = &gps,
    .lora = &lora,
    .register_ui_task = register_ui_task,
    .register_service_task = register_service_task,
    .time_of_last_action = time_of_last_action_getter,
    .light_sleep = light_sleep,
};

void emuBoardInit() {
    printf("[Emulator] Initializing emulator board\n");

    // SDL is initialized by EmulatorScreen::initScreen()
    // Other components are initialized by their respective init methods

    // Set initial last action time
    time_of_last_action.store(SDL_GetTicks());
}

#endif
