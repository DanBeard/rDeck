#pragma once

#include "retHal/RetHal.h"
#include "lvgl.h"

class RetOS;
class BaseScreen;


struct ServiceIcon {
    uint8_t serviceID;
    const char* icon;
    lv_opa_t opacity;
};

class RetUI {
    friend class RetOS;

    public:
        RetUI(RetOS* os);

        void init();

        void showLoadingScreen();
        void showAppScreen();

        void hideBackButton();
        void showBackButton();

        // service UI icons
        void setServiceIcon(ServiceIcon &iconInfo);
        void renderServiceIcons();

        // LVGL accessors
        lv_obj_t* app_screen() const;
        void clear_app_screen();

        // THEMINING
        // just black and white for e-ink displays for now
        inline lv_color_t fg_color() const { return lv_color_black();};
        inline lv_color_t bg_color() const { return lv_color_white();};
        inline lv_group_t* default_input_group() const {return _default_input_group;};

        // a loop function call like, maybe every few seconds? doesn't need to be fast
        void slow_loop();

    protected:
        RetOS* _retos;
        lv_obj_t* _root;
        // top bar components
        lv_obj_t* _top_bar;
        lv_obj_t *_battery;
        lv_obj_t *_service_icons;
        lv_obj_t *_back_btn;
        const char * _last_battery_symbol = LV_SYMBOL_BATTERY_EMPTY;

        // app screen componenets
        lv_obj_t* _app_screen;

        // misc
        lv_obj_t* _loading_screen;
        lv_group_t * _default_input_group;


    // components
    void drawTopBar();
    

};