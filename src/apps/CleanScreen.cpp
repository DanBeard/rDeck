#include "CleanScreen.h"

static unsigned long last_tick = 0;
static boolean last_tick_black = false;
static uint8_t cycle_count = 0;
static const uint8_t MAX_CYCLES = 5;  // Run 5 full refresh cycles
static lv_obj_t * cln_obj;
static lv_obj_t * progress_label;

/*virtual */ void CleanScreen::start(RetOS* retos) {
    // Reset state
    last_tick = 0;
    last_tick_black = false;
    cycle_count = 0;

    /*Create an object with the new style*/
    cln_obj = lv_obj_create(screen);
    lv_obj_set_height(cln_obj, lv_pct(100));
    lv_obj_set_width(cln_obj, lv_pct(100));
    lv_obj_center(cln_obj);

    // Add progress label
    progress_label = lv_label_create(cln_obj);
    lv_label_set_text(progress_label, "Cleaning...\n0/5");
    lv_obj_center(progress_label);
}

/*virtual */ void CleanScreen::tick(const unsigned long tickMillis) {
    unsigned long tickdiff = tickMillis - last_tick;

    // 1.5s between cycles (full refresh is ~1.1s, leave margin)
    if(tickdiff > 1500) {
        if(cycle_count < MAX_CYCLES) {
            // Direct GxEPD2 call bypasses LVGL for proper e-ink full refresh
            // This writes to BOTH buffers and triggers a full refresh
            // which properly resets the electrophoretic particles
            uint8_t color = last_tick_black ? 0xFF : 0x00;  // Alternate black/white
            _retos->hal().screen->forceFullRefresh(color);

            last_tick_black = !last_tick_black;
            cycle_count++;

            // Update progress label
            char buf[32];
            snprintf(buf, sizeof(buf), "Cleaning...\n%d/%d", cycle_count, MAX_CYCLES);
            lv_label_set_text(progress_label, buf);
        } else {
            // Done - show completion message
            lv_label_set_text(progress_label, "Done!\nPress back");
        }
        last_tick = tickMillis;
    }
}

/*virtual */ void CleanScreen::stop() {
    // Reset state for next time
    cycle_count = 0;
    last_tick_black = false;
}
