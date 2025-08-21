#include "Settings.h"
#include "retOS/retosUtils/timezones.h"

static JsonDocument _root_settings;

/*virtual */ void Settings::start(RetOS* retos) {

    // don't sleep while this app is open
    _keep_awake = true;

    // ensure settings are loaded
    _settings = getSettings(global_settings_section);

    drawScreen();

}

/*virtual */ void Settings::tick(const time_t tickMillis) {
    
}

/*virtual */ void Settings::stop() {
     // apply our local settiing
     JsonString timezone_val = _settings[timezone];
     time_t epoch_val = _settings[epoch];
     _retos->time.setPosixTimezone(timezone_val.c_str());
     if(epoch_val > 0) {
         _retos->time.setTime(epoch_val);
     }
    

     // apply the settings registered by services
     for(auto sInfo: _retos->serviceInfo()) {
        sInfo.applySettings();
    }

    // save and clear settings since JSON changes lead to garbage memleak
    Settings::saveSettings();
    _root_settings.clear();   
}
 


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

    serializeJsonPretty(_root_settings, Serial);

}

void Settings::drawScreen() {
    settings_column = lv_obj_create(screen);
    lv_obj_set_size(settings_column, LV_PCT(100), LV_PCT(100));
    lv_obj_align(settings_column, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(settings_column, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_left(settings_column, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(settings_column, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // date & time

    drawTimeDateSection();

    // draw any section added by services
    for(auto sInfo: _retos->serviceInfo()) {
        sInfo.drawSettings(settings_column, this);
    }

}

void functor_callback(lv_event_t * e) {
    //JsonObject _settings = Settings::getSettings(SETTINGS_SECTION);
    FunctorCallback* fcb = (FunctorCallback *) lv_event_get_user_data(e);
    // call the functor
    if(*fcb) {
        (*fcb)(e);
    }

}

void Settings::drawSettingsSectionHeader(lv_obj_t* container, const char* title) {
    //header
    lv_obj_t *settingsHeader = lv_label_create(container);
    lv_obj_set_size(settingsHeader, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_flag(settingsHeader, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_label_set_text(settingsHeader, title);

    // underline/bottom border for setting
    lv_obj_set_style_border_width(settingsHeader, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(settingsHeader, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(settingsHeader, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);

}

// returns the text area
lv_obj_t* Settings::drawSettingsTextInputRow( lv_obj_t* container, const char* title, const char* value, FunctorCallback *callback) {

    lv_obj_t *label = lv_label_create(container);
    lv_obj_set_size(label,  LV_PCT(38), LV_SIZE_CONTENT);
    lv_label_set_text(label, title);
    lv_obj_set_style_pad_top(label, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);

    lv_obj_t *ta = lv_textarea_create(container);
    lv_obj_set_size(ta,  LV_PCT(59), LV_SIZE_CONTENT);
    if(callback != nullptr) {
        lv_obj_add_event_cb(ta, functor_callback, LV_EVENT_VALUE_CHANGED, callback);
    }
    lv_textarea_set_one_line(ta, true);

    if(value != nullptr && strnlen(value, 2) > 0) {
        lv_textarea_set_text(ta, value);
    }

    lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ta, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_BOTTOM, LV_STATE_DEFAULT);

    return ta;
}

void Settings::drawTimeDateSection() {

    drawSettingsSectionHeader(settings_column, "Date/Time");
    
    JsonString timezone_val = _settings[timezone];

    time_t now;
    time(&now);

    String epoch_val(now);

    // MUST be static so it exists past function call
    static FunctorCallback timezone_callback;
    // needs to be re-inited every call
    timezone_callback = [this](lv_event_t *e){
        lv_obj_t * ta = lv_event_get_target(e);
        // don't pass raw char* to JsonArduino or it won't copy them adn you'll get junk later
        String new_timezone = lv_textarea_get_text(ta);
        new_timezone.toUpperCase(); // timezones are always all upper case
        _settings[timezone] = new_timezone;
    }; 


    static FunctorCallback epoch_callback;
    // needs to be re-inited every call
    epoch_callback = [this](lv_event_t *e){
        lv_obj_t * ta = lv_event_get_target(e);
        // don't pass raw char* to JsonArduino or it won't copy them adn you'll get junk later
        String new_epoch = lv_textarea_get_text(ta);
        _settings[epoch] = new_epoch.toInt();
    }; 

    Settings::drawSettingsTextInputRow(settings_column, "Timezone", timezone_val.c_str(), &timezone_callback);
    Settings::drawSettingsTextInputRow(settings_column, "Epoch", epoch_val.c_str(), &epoch_callback);

    // TODO: Timezone lookup modal by city search or using GPS logic
    // lv_obj_t * btn1 = lv_btn_create(settings_column);
    // lv_obj_add_flag(btn1, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    // //lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    // //lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);


    // lv_obj_t* btn_label = lv_label_create(btn1);
    // lv_label_set_text(btn_label, "Lookup Posix Timezone");
    // lv_obj_center(btn_label);


    
}