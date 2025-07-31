#include "retUI.h"
#include "retHal/Screen/BaseScreen.h"
#include "retOS.h"
#include <Arduino.h>

RetUI::RetUI(RetOS* os) : _os(os) {
    _hal = os->hal();
    // we're gunna be using the screen a bunch so just cache it here
    _screen = _hal.screen;
}

void RetUI::init() {
    _root = lv_obj_create(NULL);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    // top bar with status like battery
    _top_bar = lv_obj_create(_root);
    lv_obj_set_size(_top_bar, lv_pct(100), 20);
    lv_obj_set_pos(_top_bar, 0, 0);
    lv_obj_set_style_border_width(_top_bar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(_top_bar, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(_top_bar, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);

    lv_obj_t *label = lv_label_create(_top_bar);
    lv_obj_set_width(label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(label, 0);
    lv_obj_set_y(label, 5);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(label, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "This is the top bar! -----");

    // scrollable we let apps draw to
    _app_screen = lv_obj_create(_root);
    lv_obj_set_size(_app_screen, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_top(_app_screen, 15, LV_PART_MAIN | LV_STATE_DEFAULT);


    // loading...
    lv_obj_t *label2 = lv_label_create(_app_screen);
    lv_obj_set_width(label2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(label2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(label2, 0);
    lv_obj_set_y(label2, 0);
    lv_obj_set_align(label2, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(label2, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label2, "Loading...");

    Serial.println("Initing UI...");
    lv_scr_load(_root);
}

lv_obj_t* RetUI::app_screen() const {
    return _app_screen;
}
void RetUI::clear_app_screen() {
    return lv_obj_clean(_app_screen);
}

