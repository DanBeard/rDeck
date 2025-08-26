#include "retUI.h"
#include "retHal/Screen/BaseScreen.h"
#include "retOS.h"
#include <Arduino.h>
#include "apps/BaseApp.h"

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
    lv_obj_set_style_pad_top(_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_obj_set_style_pad_top(_app_screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(_app_screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(_app_screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(_app_screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);


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
    BaseApp * active_app =  retOsGlobalPtr->activeApp();
    if(active_app != nullptr) {
        const function<void()> custom_back_action = active_app->customBackButtonAction();
        if(custom_back_action) {
            custom_back_action();
            return;
        }
    }
    // Default -- close app and go back to launcher
    retOsGlobalPtr->backToLauncher();
}

void RetUI::drawTopBar() {
         // top bar with status like battery
    _top_bar = lv_obj_create(_root);
    lv_obj_set_size(_top_bar, lv_pct(100), 25);
    //lv_obj_set_pos(_top_bar, 0, 0);
    lv_obj_set_flex_grow(_top_bar, 0);
    lv_obj_set_style_pad_top(_top_bar, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_label_set_text(_battery, LV_SYMBOL_REFRESH);
    lv_obj_set_width(_battery, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(_battery, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(_battery, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(_battery, 0,10);
    lv_obj_set_style_text_color(_battery, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(_battery, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    

    _service_icons = lv_obj_create(_top_bar);
    lv_obj_set_width(_service_icons, 85);   /// 1
    lv_obj_set_height(_service_icons, 22);
    //lv_obj_set_pos(_service_icons, 85, 0);
    lv_obj_align_to(_service_icons, _battery, LV_ALIGN_OUT_LEFT_BOTTOM, 5, 4);
    lv_obj_clear_flag(_service_icons, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_pad_top(_service_icons, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_layout(_service_icons, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_service_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_service_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);

    //lv_obj_set_style_border_width(_service_icons, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    //lv_obj_set_style_border_color(_service_icons, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_move_foreground(_battery);

    // render any service icons that got registered before we created the bar
    renderServiceIcons();

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

// max 6 service icons
#define NUM_SERVICE_ICONS 6
ServiceIcon serviceIcons[NUM_SERVICE_ICONS] = {0};

void RetUI::setServiceIcon(ServiceIcon &iconInfo) {
    // assume that we grow from first to last
    // and that we never delete service icons (just set opacity to 0)
    // so if we find one where id =0, boom take it
    for(int i=0; i<NUM_SERVICE_ICONS; i++){
        if(serviceIcons[i].serviceID == iconInfo.serviceID || serviceIcons[i].serviceID == 0){
            serviceIcons[i] = iconInfo;
            i = NUM_SERVICE_ICONS + 1; // break
        }
    }

    renderServiceIcons();
}

void RetUI::renderServiceIcons() {
    if(_service_icons) {
        lv_obj_clean(_service_icons);
        lv_coord_t x = lv_obj_get_width(_service_icons);
        for(int i=0; i<NUM_SERVICE_ICONS; i++){
            if(serviceIcons[i].serviceID > 0) {
                Serial.printf("Service ICON op= %u \n", serviceIcons[i].opacity);
                lv_obj_t *icon = lv_label_create(_service_icons);
                //lv_obj_set_width(icon, 25);   /// 1
                //lv_obj_set_height(icon, 25);
                //lv_obj_set_pos(_service_icons, x, 0);
                x-=25;
                //lv_obj_set_flex_grow(icon, 0);
                lv_label_set_text(icon,serviceIcons[i].icon);
                lv_obj_set_style_text_color(icon, fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_opa(icon, serviceIcons[i].opacity, LV_PART_MAIN | LV_STATE_DEFAULT);
            }  
    }
    }
}


void RetUI::hideBackButton() {
    lv_obj_add_flag(_back_btn, LV_OBJ_FLAG_HIDDEN);

}
void RetUI::showBackButton(){
    lv_obj_clear_flag(_back_btn, LV_OBJ_FLAG_HIDDEN);
}


