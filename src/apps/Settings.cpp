#include "Settings.h"
#include "retOS/retosUtils/timezones.h"


#define SETTINGS_SECTION "settings"

/*virtual */ void Settings::start(RetOS* retos) {

    // don't sleep while this app is open
    _keep_awake = true;

    // ensure settings are loaded
    _settings = getSettings(SETTINGS_SECTION);


    drawScreen();

}

/*virtual */ void Settings::tick() {
    
}

/*virtual */ void Settings::stop() {
     Settings::saveSettings();
    
}
 
static JsonDocument _root_settings;

JsonObject Settings::getSettings(const char* section) {
    // if we're null, then load it
    if(_root_settings.isNull()) {
        Serial.println("Loading settings doc....");
        FS* fs = retOsGlobalPtr->hal().fs;
        if(!fs->exists(Settings::settingsFile)){
            Serial.println("Creating settings doc....");
            File file = fs->open(Settings::settingsFile, FILE_WRITE);
            if(!file){
                Serial.println("Failed to open settings file for writing");
                return JsonObject();
            }

            // Start with just an empty settings file
            file.write((uint8_t*)"{}", strnlen("{}", 16));
            file.close();
        }
        Serial.println("Opening settings doc....");
        // file should exist here
        File file = fs->open(Settings::settingsFile, FILE_READ);
        if(!file){
                Serial.println("Failed to open settings file for reading");
                return JsonObject();
            }
    
        auto status = deserializeJson(_root_settings, file);
        Serial.println("Done Deserializing....");
        if(status != DeserializationError::Ok) {
            Serial.print("Error deserializing settings file: ");
            Serial.println(status.c_str());
        }
        file.close();
    }

    if(!_root_settings.containsKey(section)) {
        Serial.println("Creating section");
        _root_settings[section]["auto_created"] = true;
    }
    
    Serial.println("Returning section");
    return _root_settings[section];
}

void Settings::saveSettings(){
    FS* fs = retOsGlobalPtr->hal().fs;
    File file = fs->open(Settings::settingsFile, FILE_WRITE);
    auto status = serializeJsonPretty(_root_settings, file);
     Serial.print("Writing settings. Wrote:  ");
     Serial.print(status);

    file.close();

    // back to null so it'll reload
    _root_settings.clear();
}

void Settings::drawScreen() {
    settings_column = lv_obj_create(screen);
    lv_obj_set_size(settings_column, LV_PCT(100), LV_PCT(100));
    lv_obj_align(settings_column, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(settings_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_left(settings_column, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(settings_column, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // date & time

    drawTimeDateSection();

}

static lv_obj_t * timezone_ta;
void timezone_callback(lv_event_t * e) {
    JsonObject _settings = Settings::getSettings(SETTINGS_SECTION);
    lv_obj_t * ta = timezone_ta; //lv_event_get_target(e);
    const char* new_timezone = lv_textarea_get_text(ta);
    Serial.print("Timezone=");
    Serial.println(new_timezone);
    _settings["timezone"] = new_timezone;
}

void Settings::drawTimeDateSection() {

        Serial.println("DrawDatetimeStart....");
    //header
    lv_obj_t *settingsHeader = lv_label_create(settings_column);
    lv_obj_set_size(settingsHeader, LV_PCT(100), LV_SIZE_CONTENT);
    lv_label_set_text(settingsHeader, "Date/Time");

    // underline/bot border for setting
    lv_obj_set_style_border_width(settingsHeader, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(settingsHeader,_retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(settingsHeader, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);


    // timezone selection
    lv_obj_t* row = lv_obj_create(settings_column);
    lv_obj_add_flag(row, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_set_size(row,  LV_PCT(100), LV_SIZE_CONTENT);

    // no padding    
    lv_obj_set_style_pad_left(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    //lv_obj_align(settings_column, LV_ALIGN_TOP_MID, 0, 5);;
    lv_obj_set_flex_flow(settings_column, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    // border for debug

    lv_obj_t *label = lv_label_create(row);
    lv_obj_set_size(label,  LV_PCT(33), LV_SIZE_CONTENT);
    lv_label_set_text(label, "Timezone");

    lv_obj_t *ta = lv_textarea_create(row);
    timezone_ta = ta;
    lv_obj_set_size(ta,  LV_PCT(65), LV_SIZE_CONTENT);
    lv_obj_add_event_cb(ta, timezone_callback, LV_EVENT_VALUE_CHANGED, NULL);
    lv_textarea_set_placeholder_text(ta, "POSIX Style");
    lv_textarea_set_one_line(ta, true);
    JsonString timezone_val = _settings["timezone"];
    if(timezone_val.c_str() != nullptr && timezone_val.size() > 0) {
        lv_textarea_set_text(ta, timezone_val.c_str());
    }


    lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ta,_retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);


    // Lookup
    lv_obj_t * btn1 = lv_btn_create(settings_column);
    lv_obj_add_flag(btn1, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    //lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    //lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);


    lv_obj_t* btn_label = lv_label_create(btn1);
    lv_label_set_text(label, "Lookup Posix Timezone");
    lv_obj_center(label);


    
}