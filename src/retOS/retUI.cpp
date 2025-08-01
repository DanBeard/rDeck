#include "retUI.h"
#include "retHal/Screen/BaseScreen.h"
#include "retOS.h"
#include <Arduino.h>

RetUI::RetUI(RetOS* os) : _retos(os) {
     
}

void RetUI::init() {

     // Input group logic
    _default_input_group = lv_group_create(); // input group for keyboards and stuff
    lv_group_set_default(_default_input_group);
        // link keyboard to default lvgl input group
    lv_indev_set_group(_retos->hal().keyboard->indev(), _default_input_group);

    // loading screen
    _loading_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(_loading_screen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    

    // loading... until an app re-draws
    lv_obj_t *loading_label = lv_label_create(_loading_screen);
    lv_obj_set_width(loading_label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(loading_label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(loading_label, 0);
    lv_obj_set_y(loading_label, 0);
    lv_obj_set_align(loading_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(loading_label, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(loading_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(loading_label, "Loading...");


    _root = lv_obj_create(NULL);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_layout(_root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_root, LV_FLEX_FLOW_COLUMN);
     lv_obj_set_flex_align(_root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // top bar on....top of app canvas
    drawTopBar();

    // scrollable we let apps draw to
    _app_screen = lv_obj_create(_root);
    //lv_obj_set_size(_app_screen, lv_pct(100), lv_pct(100));
    lv_obj_set_width(_app_screen, lv_pct(100));
    lv_obj_set_flex_grow(_app_screen, 1);
    //lv_obj_set_style_pad_top(_app_screen, 22, LV_PART_MAIN | LV_STATE_DEFAULT);


    Serial.println("Initing UI...");
    lv_scr_load(_root);
}

void RetUI::showLoadingScreen(){
    lv_scr_load(_loading_screen);
}

void RetUI::showAppScreen(){
    lv_scr_load(_root);
}

lv_obj_t* RetUI::app_screen() const {
    return _app_screen;
}
void RetUI::clear_app_screen() {
    return lv_obj_clean(_app_screen);
}


static void back_btn_event_cb(lv_event_t *e) {
    Serial.println("Back to launcher baby!!");
    retOsGlobalPtr->backToLauncher();
}

void RetUI::drawTopBar() {
         // top bar with status like battery
    _top_bar = lv_obj_create(_root);
    lv_obj_set_size(_top_bar, lv_pct(100), 20);
    //lv_obj_set_pos(_top_bar, 0, 0);
    lv_obj_set_flex_grow(_top_bar, 0);
    lv_obj_set_style_border_width(_top_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(_top_bar, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(_top_bar, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);
    lv_obj_clear_flag(_top_bar, LV_OBJ_FLAG_SCROLLABLE);

    _back_btn = lv_label_create(_top_bar);
    lv_obj_set_width(_back_btn, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(_back_btn, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(_back_btn, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_color(_back_btn, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(_back_btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(_back_btn, LV_SYMBOL_NEW_LINE);
    lv_obj_add_flag(_back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_back_btn, back_btn_event_cb, LV_EVENT_CLICKED, (void *)(0));
    lv_obj_set_ext_click_area(_back_btn, 20);

    _battery = lv_label_create(_top_bar);
    lv_obj_set_width(_battery, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(_battery, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(_battery, LV_ALIGN_RIGHT_MID);
    lv_obj_set_style_text_color(_battery, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(_battery, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(_battery, LV_SYMBOL_REFRESH);


}

void RetUI::slow_loop() {
    // battery update
    BaseBattery * bat = _retos->hal().battery;
    if(bat) {
        uint8_t percent = bat->percent();
        bool charging = bat->charging();

        const char * sym = LV_SYMBOL_WARNING;
        if(charging) {
            sym = LV_SYMBOL_CHARGE;
        } else {
            // char percentNum[100];
            // sprintf(percentNum,"%u",percent);
            // sym=percentNum;
            if(percent < 20) sym = LV_SYMBOL_BATTERY_EMPTY;
            else if(percent < 40) sym = LV_SYMBOL_BATTERY_1;
            else if(percent < 69) sym = LV_SYMBOL_BATTERY_2;
            else if(percent < 92) sym = LV_SYMBOL_BATTERY_3;
            else sym = LV_SYMBOL_BATTERY_FULL;
        }
        
        if(sym != _last_battery_symbol) {
            _last_battery_symbol = sym;
            lv_label_set_text(_battery, sym);
        }
    }
}



void RetUI::hideBackButton() {
    lv_obj_add_flag(_back_btn, LV_OBJ_FLAG_HIDDEN);

}
void RetUI::showBackButton(){
    lv_obj_clear_flag(_back_btn, LV_OBJ_FLAG_HIDDEN);
}


