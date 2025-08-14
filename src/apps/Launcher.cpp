#include "Launcher.h"
#include "retOS/retOS.h"
#include "lvgl.h"
#include "assets/assets.h"

/*virtual */ void Launcher::start(RetOS* retos) {

    _ui = retos->ui();

    // draw all of the apps
    uint8_t i = 0;
    for (AppInfo app: retos->appInfo()) {
        uint8_t x = 23+(i%3*72);
        uint8_t y = 23 + (i/3) * 88;
        Serial.printf("Creating btn for %s  %d - %d, %d", app.name, i,x, y );
        Serial.println("");
        menu_btn_create(screen, app, x, y);
        i++;
    }

}

/*virtual */ void Launcher::tick() {
    
}

/*virtual */ void Launcher::stop() {
    
}

static void menu_btn_event_cb(lv_event_t *e)
{
    uint32_t id = (uint32_t)e->user_data;
    Serial.print("APP CLICKED=");
    Serial.println(id);
    // lv_label_set_text(label, tgr->name);
    // gotta use the global ptr in a c-style callback
    retOsGlobalPtr->launchApp(id);
}

void Launcher::menu_btn_create(lv_obj_t *parent, AppInfo& appInfo, int x, int y)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_width(btn, 50);
    lv_obj_set_height(btn, 50);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_OVERFLOW_VISIBLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(btn, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 3, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_width(label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(label, 0);
    lv_obj_set_y(label, 20);
    lv_obj_set_align(label, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_x(btn, x);
    lv_obj_set_y(btn, y);
    const void *icon = &img_touch;
    if(appInfo.icon) icon = appInfo.icon;
    lv_obj_set_style_bg_img_src(btn,icon, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, appInfo.name);
    lv_obj_set_style_border_width(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    //launch on click
    uint32_t id_hax = appInfo.id;
    lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, (void *)(id_hax));
    
}