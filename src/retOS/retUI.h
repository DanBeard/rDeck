#pragma once

#include "retHal/RetHal.h"
#include "lvgl.h"

class RetOS;
class BaseScreen;

class RetUI {
    friend class RetOS;

    public:
        RetUI(RetOS* os);

        void init();

        // LVGL accessors
        lv_obj_t* app_screen() const;
        void clear_app_screen();
        // THEMINING
        // just black and white for e-ink displays for now
        inline lv_color_t fg_color() const { return lv_color_black();};
        inline lv_color_t bg_color() const { return lv_color_white();};
        inline lv_group_t* default_input_group() const {return _default_input_group;};

    protected:
        RetOS* _retos;
        lv_obj_t* _root;
        lv_obj_t* _top_bar;
        lv_obj_t* _app_screen;
        lv_group_t * _default_input_group;


    // components
    void drawTopBar();
    

};