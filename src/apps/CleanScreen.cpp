#include "CleanScreen.h"


static unsigned long last_tick = 0;
static boolean last_tick_black = false;
static lv_obj_t * cln_obj;

/*virtual */ void CleanScreen::start(RetOS* retos) {
    
    /*Create an object with the new style*/
    cln_obj = lv_obj_create(screen);
    lv_obj_set_height(cln_obj, lv_pct(100));
    lv_obj_set_width(cln_obj, lv_pct(100));
    lv_obj_center(cln_obj);

}

/*virtual */ void CleanScreen::tick(const unsigned long tickMillis) {
    
    unsigned long tickdiff = tickMillis - last_tick;
    if(tickdiff % 100 ==0 ) {
        
        static lv_style_t style;
        lv_style_init(&style);

        lv_style_set_bg_opa(&style, LV_OPA_COVER);
        lv_style_set_bg_color(&style, last_tick_black ? lv_color_white() : lv_color_black());
        lv_obj_add_style(cln_obj, &style, 0);
    }
    if(tickdiff > 5500) {
        last_tick = tickMillis;
        last_tick_black = !last_tick_black;
    }
    
}

/*virtual */ void CleanScreen::stop() {
    
}
