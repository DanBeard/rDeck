/**
 * main.cpp - Entry point for the emulator build.
 * Provides main() that calls Arduino's setup() and loop().
 *
 * Command-line options:
 *   --launch-app <name>  Auto-launch an app after startup (e.g., Settings, uChat)
 *   --no-gui             Run without GUI (headless mode, useful for testing)
 *   --max-frames <n>     Exit after n frames (for automated testing)
 */

#ifdef RET_PLATFORM_EMU

#include <Arduino.h>
#include <SDL2/SDL.h>
#include <cstring>
#include <cstdlib>

// External Arduino-style entry points defined in rdeck.ino
extern void setup();
extern void loop();

// Global for auto-launch app name (set from command line)
const char* g_autoLaunchApp = nullptr;
int g_maxFrames = 0;  // 0 means unlimited

// Called by RetOS after apps are initialized to check if we should auto-launch
extern "C" const char* emu_get_autolaunch_app() {
    return g_autoLaunchApp;
}

#ifdef DEBUG
#include <exception>
#include <execinfo.h>
#include <cxxabi.h>

// Custom terminate handler - prints stack trace before aborting
static void custom_terminate() {
    fprintf(stderr, "\n=== FATAL: std::terminate() called ===\n");

    if (auto eptr = std::current_exception()) {
        try {
            std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            fprintf(stderr, "Exception: %s\n", e.what());
        } catch (...) {
            fprintf(stderr, "Unknown exception type\n");
        }
    }

    fprintf(stderr, "Stack trace:\n");
    void* frames[64];
    int count = backtrace(frames, 64);
    char** symbols = backtrace_symbols(frames, count);
    if (symbols) {
        for (int i = 0; i < count; i++) {
            fprintf(stderr, "  [%d] %s\n", i, symbols[i]);
        }
        free(symbols);
    }

    fprintf(stderr, "=== END TRACE ===\n");
    fflush(stderr);
    abort();
}
#endif

static void printUsage(const char* progname) {
    printf("Usage: %s [options]\n", progname);
    printf("Options:\n");
    printf("  --launch-app <name>  Auto-launch app after startup (e.g., Settings, uChat, Clock)\n");
    printf("  --max-frames <n>     Exit after n frames (for automated testing)\n");
    printf("  --help               Show this help\n");
}

int main(int argc, char* argv[]) {
#ifdef DEBUG
    std::set_terminate(custom_terminate);
#endif

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--launch-app") == 0 && i + 1 < argc) {
            g_autoLaunchApp = argv[++i];
            printf("[Emulator] Will auto-launch app: %s\n", g_autoLaunchApp);
        } else if (strcmp(argv[i], "--max-frames") == 0 && i + 1 < argc) {
            g_maxFrames = atoi(argv[++i]);
            printf("[Emulator] Will exit after %d frames\n", g_maxFrames);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            printf("[Emulator] Unknown argument: %s\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    printf("[Emulator] Starting rDeck emulator\n");

    // Call Arduino setup
    setup();

    // Main loop - runs until SDL_QUIT event
    printf("[Emulator] Entering main loop\n");
    bool running = true;
    SDL_Event event;
    int frameCount = 0;

    while (running) {
        // Process SDL events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Call Arduino loop
        loop();

        frameCount++;

        // Check max frames limit
        if (g_maxFrames > 0 && frameCount >= g_maxFrames) {
            printf("[Emulator] Reached max frames (%d), exiting\n", g_maxFrames);
            running = false;
        }

        // Small delay to avoid burning CPU
        SDL_Delay(10);
    }

    printf("[Emulator] Shutting down\n");
    SDL_Quit();

    return 0;
}

#endif // RET_PLATFORM_EMU
