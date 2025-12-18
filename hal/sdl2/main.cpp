/**
 * main.cpp - Entry point for the emulator build.
 * Provides main() that calls Arduino's setup() and loop().
 */

#ifdef RET_PLATFORM_EMU

#include <Arduino.h>
#include <SDL2/SDL.h>

// External Arduino-style entry points defined in rdeck.ino
extern void setup();
extern void loop();

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("[Emulator] Starting rDeck emulator\n");

    // Call Arduino setup
    setup();

    // Main loop - runs until SDL_QUIT event
    printf("[Emulator] Entering main loop\n");
    bool running = true;
    SDL_Event event;

    while (running) {
        // Process SDL events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Call Arduino loop
        loop();

        // Small delay to avoid burning CPU
        SDL_Delay(10);
    }

    printf("[Emulator] Shutting down\n");
    SDL_Quit();

    return 0;
}

#endif // RET_PLATFORM_EMU
