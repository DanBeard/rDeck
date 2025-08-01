#include "retUI.h"
#include "retHal/Screen/BaseScreen.h"
#include "retOS.h"
#include <Arduino.h>

RetUI::RetUI(RetOS* os) : _retos(os) {
     
}

void RetUI::init() {
    _default_input_group = lv_group_create(); // input group for keyboards and stuff
    lv_group_set_default(_default_input_group);
        // link keyboard to default lvgl input group
    lv_indev_set_group(_retos->hal().keyboard->indev(), _default_input_group);

    _root = lv_obj_create(NULL);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    // scrollable we let apps draw to
    _app_screen = lv_obj_create(_root);
    lv_obj_set_size(_app_screen, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_top(_app_screen, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    // loading... until an app re-draws
    lv_obj_t *label2 = lv_label_create(_app_screen);
    lv_obj_set_width(label2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(label2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(label2, 0);
    lv_obj_set_y(label2, 0);
    lv_obj_set_align(label2, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(label2, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label2, "Loading...");

    // top bar on....top of app canvas
    drawTopBar();

    Serial.println("Initing UI...");
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
    lv_obj_set_pos(_top_bar, 0, 0);
    lv_obj_set_style_border_width(_top_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(_top_bar, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(_top_bar, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);
    lv_obj_clear_flag(_top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *backl = lv_label_create(_top_bar);
    lv_obj_set_width(backl, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(backl, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(backl, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_color(backl, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(backl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(backl, LV_SYMBOL_NEW_LINE);
    lv_obj_add_flag(backl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(backl, back_btn_event_cb, LV_EVENT_CLICKED, (void *)(0));
    lv_obj_set_ext_click_area(backl, 20);

    lv_obj_t *batl = lv_label_create(_top_bar);
    lv_obj_set_width(batl, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(batl, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(batl, LV_ALIGN_RIGHT_MID);
    lv_obj_set_style_text_color(batl, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(batl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(batl, LV_SYMBOL_BATTERY_EMPTY);


}

