#include "Clock.h"
#include "sys/time.h"


/*virtual */ void ClockApp::start(RetOS* retos) {
    _time_lbl = lv_label_create(screen);
    lv_obj_set_width(_time_lbl, LV_SIZE_CONTENT);   
    lv_obj_set_height(_time_lbl, LV_SIZE_CONTENT);    
    lv_obj_set_x(_time_lbl, 0);
    lv_obj_set_y(_time_lbl, 0);
    lv_obj_set_align(_time_lbl, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(_time_lbl, retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(_time_lbl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(_time_lbl, "");

    _date_lbl = lv_label_create(screen);
    lv_obj_set_width(_date_lbl, LV_SIZE_CONTENT);   
    lv_obj_set_height(_date_lbl, LV_SIZE_CONTENT);  
    lv_obj_set_x(_date_lbl, 0);
    lv_obj_set_y(_date_lbl, 20);
    lv_obj_set_align(_date_lbl, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(_date_lbl, retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(_date_lbl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(_date_lbl, "");

    updateTime();

}

/*virtual */ void ClockApp::tick() {
    updateTime();
}

/*virtual */ void ClockApp::stop() {
    
}

void ClockApp::updateTime() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if(now - _last_time_grabbed > update_every_sec) {
        char time_txt[20];
        char date_txt[20];
        strftime(time_txt, 20, "%H:%M:%S", &timeinfo);
        strftime(date_txt, 20, "%Y-%m-%d", &timeinfo);
        sprintf(time_txt, "%.2d:%.2d", _last_time_info.tm_hour, _last_time_info.tm_min);
        sprintf(date_txt, "%d-%.2d-%.2d", _last_time_info.tm_year, _last_time_info.tm_mon, _last_time_info.tm_mday);

        lv_label_set_text(_time_lbl, time_txt);
        lv_label_set_text(_date_lbl, date_txt);

        Serial.println(time_txt);
        Serial.println(date_txt);
        Serial.println(now);
        _last_time_grabbed = now;
    }

}
