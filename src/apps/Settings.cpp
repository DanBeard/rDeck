#include "Settings.h"
#include "retOS/retosUtils/timezones.h"




/*virtual */ void Settings::start(RetOS* retos) {

    drawScreen();

    // lv_obj_t *label2 = lv_label_create(screen);
    // lv_obj_set_width(label2, LV_SIZE_CONTENT);   /// 1
    // lv_obj_set_height(label2, LV_SIZE_CONTENT);    /// 1
    // lv_obj_set_x(label2, 0);
    // lv_obj_set_y(label2, 0);
    // lv_obj_set_align(label2, LV_ALIGN_CENTER);
    // lv_obj_set_style_text_color(label2, retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_label_set_text(label2, "Hello World!");

}

/*virtual */ void Settings::tick() {
    
}

/*virtual */ void Settings::stop() {
    
}


void Settings::drawScreen() {
    settings_column = lv_obj_create(screen);
    lv_obj_set_size(settings_column, 200, 150);
    lv_obj_align(settings_column, LV_ALIGN_TOP_MID, 0, 5);;
    lv_obj_set_flex_flow(settings_column, LV_FLEX_FLOW_COLUMN);
    // date & time
    drawTimeDateSection();

}
void Settings::drawTimeDateSection() {
    //header
    lv_obj_t *settingsHeader = lv_label_create(settings_column);
    lv_obj_set_size(settingsHeader, LV_PCT(100), LV_SIZE_CONTENT);
    lv_label_set_text(settingsHeader, "Date/Time");

    // timezone dropdown
    lv_obj_t* row = lv_obj_create(settings_column);
    lv_obj_set_size(row,  LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(settings_column, LV_ALIGN_TOP_MID, 0, 5);;
    lv_obj_set_flex_flow(settings_column, LV_FLEX_FLOW_ROW);

    lv_obj_t *label = lv_label_create(row);
    lv_obj_set_size(label, LV_PCT(50), LV_SIZE_CONTENT);
    lv_label_set_text(label, "Timezone");

    lv_obj_t *dropdown = lv_dropdown_create(row);
    lv_obj_set_size(dropdown, LV_PCT(50), LV_SIZE_CONTENT);
    lv_dropdown_set_options_static(dropdown, timezone_dropdown_options);

    
}