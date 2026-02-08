#include "Settings.h"
#include "retOS/retosUtils/timezones.h"
#include "services/RnsUtils/TrustedServers.h"
#include "services/RnsService.h"
#include "services/TimeService.h"

static JsonDocument _root_settings;

/*virtual */ void Settings::start(RetOS* retos) {

    // don't sleep while this app is open
    _keep_awake = true;

    // ensure settings are loaded
    _settings = getSettings(global_settings_section);

    drawScreen();

}

/*virtual */ void Settings::tick(const unsigned long tickMillis) {
    
}

/*virtual */ void Settings::stop() {
    // apply our local settings (with null checks to avoid crashes)
    JsonString timezone_val = _settings[timezone];
    time_t epoch_val = _settings[epoch];

    if (!timezone_val.isNull()) {
        _retos->time.setPosixTimezone(timezone_val.c_str());
    }

    if (epoch_val > 0) {
        _retos->time.setTime(epoch_val);
    }

    // apply the settings registered by services
    for (auto sInfo: _retos->serviceInfo()) {
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

    // Trusted servers section
    drawTrustedServersSection();

    // Device identity section (regenerate identity button)
    drawDeviceIdentitySection();

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

// Callback data for trust accept/revoke buttons
struct TrustButtonData {
    std::string hashHex;
    Settings* settings;
};

static void trustAcceptCallback(lv_event_t* e) {
    TrustButtonData* data = (TrustButtonData*)lv_event_get_user_data(e);
    if (data) {
        Serial.printf("[Settings] Accepting trust for %s\n", data->hashHex.c_str());

        // Accept the trust offer
        if (Retcon::Service::getTrustedServers().acceptOffer(data->hashHex)) {
            // Send TRUST_ACCEPT message
            RNS::Bytes serverHash;
            serverHash.assignHex(data->hashHex.c_str());

            RnsService* rns = retOsGlobalPtr->fetchService<RnsService>();
            if (rns) {
                rns->sendTrustAccept(serverHash);
            }

            // If server offers NTP, request time sync via TimeService
            auto server = Retcon::Service::getTrustedServers().getServer(data->hashHex);
            if (server) {
                for (const auto& svc : server->services) {
                    if (svc == "ntp") {
                        Serial.println("[Settings] Server offers NTP, requesting time sync...");
                        TimeService* timeSvc = retOsGlobalPtr->fetchService<TimeService>();
                        if (timeSvc) {
                            timeSvc->requestNtpSync();
                        }
                        break;
                    }
                }
            }

            // Redraw the section to show updated state
            data->settings->redrawTrustedServersSection();
        }
    }
}

static void trustRevokeCallback(lv_event_t* e) {
    TrustButtonData* data = (TrustButtonData*)lv_event_get_user_data(e);
    if (data) {
        Serial.printf("[Settings] Revoking trust for %s\n", data->hashHex.c_str());
        Retcon::Service::getTrustedServers().revokeTrust(data->hashHex);

        // Redraw the section
        data->settings->redrawTrustedServersSection();
    }
}

void Settings::drawTrustedServersSection() {
    drawSettingsSectionHeader(settings_column, "Trusted Servers");

    auto& trustedServers = Retcon::Service::getTrustedServers();
    auto pending = trustedServers.getPendingOffers();
    auto trusted = trustedServers.getTrustedServers();

    if (pending.empty() && trusted.empty()) {
        lv_obj_t* emptyLabel = lv_label_create(settings_column);
        lv_label_set_text(emptyLabel, "No server connections yet.");
        lv_obj_set_size(emptyLabel, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(emptyLabel, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_set_style_text_color(emptyLabel, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);
        return;
    }

    // Draw pending offers
    if (!pending.empty()) {
        lv_obj_t* pendingLabel = lv_label_create(settings_column);
        lv_label_set_text(pendingLabel, LV_SYMBOL_BELL " Pending Offers:");
        lv_obj_set_size(pendingLabel, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(pendingLabel, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_set_style_pad_top(pendingLabel, 5, LV_PART_MAIN);
        lv_obj_set_style_text_color(pendingLabel, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);

        for (const auto& server : pending) {
            drawTrustedServerRow(server, true);
        }
    }

    // Draw trusted servers
    if (!trusted.empty()) {
        lv_obj_t* trustedLabel = lv_label_create(settings_column);
        lv_label_set_text(trustedLabel, LV_SYMBOL_OK " Trusted:");
        lv_obj_set_size(trustedLabel, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(trustedLabel, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_set_style_pad_top(trustedLabel, 5, LV_PART_MAIN);
        lv_obj_set_style_text_color(trustedLabel, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);

        for (const auto& server : trusted) {
            drawTrustedServerRow(server, false);
        }
    }
}

void Settings::drawTrustedServerRow(const Retcon::Service::TrustedServer& server, bool isPending) {
    // Name label - takes most of the width
    lv_obj_t* nameLabel = lv_label_create(settings_column);
    lv_label_set_text(nameLabel, server.name.c_str());
    lv_obj_set_size(nameLabel, LV_PCT(55), LV_SIZE_CONTENT);
    lv_obj_add_flag(nameLabel, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_set_style_pad_top(nameLabel, 5, LV_PART_MAIN);
    lv_obj_set_style_text_color(nameLabel, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);

    // Button (Accept or Revoke) - e-paper compatible styling, larger size
    lv_obj_t* btn = lv_btn_create(settings_column);
    lv_obj_set_size(btn, LV_PCT(40), 35);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, retOsGlobalPtr->ui()->bg_color(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 5, LV_PART_MAIN);

    lv_obj_t* btnLabel = lv_label_create(btn);
    lv_label_set_text(btnLabel, isPending ? LV_SYMBOL_OK " Accept" : LV_SYMBOL_CLOSE " Revoke");
    lv_obj_set_style_text_color(btnLabel, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);
    lv_obj_center(btnLabel);

    // Store button data - use static storage to persist past function call
    static std::vector<TrustButtonData> buttonDataStorage;
    buttonDataStorage.push_back({server.hashHex(), this});
    TrustButtonData* data = &buttonDataStorage.back();

    if (isPending) {
        lv_obj_add_event_cb(btn, trustAcceptCallback, LV_EVENT_CLICKED, data);
    } else {
        lv_obj_add_event_cb(btn, trustRevokeCallback, LV_EVENT_CLICKED, data);
    }
}

// Async callback to redraw after event processing completes
static void asyncRedrawCallback(void* settings_ptr) {
    Settings* settings = (Settings*)settings_ptr;
    if (settings) {
        settings->doRedrawTrustedServersSection();
    }
}

void Settings::redrawTrustedServersSection() {
    // Defer redraw to avoid destroying objects while their event callbacks are running
    // This prevents use-after-free when the button that triggered the event is deleted
    lv_async_call(asyncRedrawCallback, this);
}

void Settings::doRedrawTrustedServersSection() {
    // Simple approach: just redraw the entire screen
    // A more sophisticated approach would track the section container and only redraw that
    lv_obj_clean(screen);
    drawScreen();
}

// Callback data for identity regeneration button
struct IdentityButtonData {
    Settings* settings;
    bool confirmPending;  // Simple "press twice" confirmation
};

static IdentityButtonData identityBtnData = {nullptr, false};

static void identityRegenCallback(lv_event_t* e) {
    IdentityButtonData* data = (IdentityButtonData*)lv_event_get_user_data(e);
    if (!data) return;

    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* label = lv_obj_get_child(btn, 0);

    if (!data->confirmPending) {
        // First press - ask for confirmation
        data->confirmPending = true;
        if (label) {
            lv_label_set_text(label, LV_SYMBOL_WARNING " Press again to confirm");
        }
        Serial.println("[Settings] Identity regeneration: waiting for confirmation");
        return;
    }

    // Second press - perform the reset
    Serial.println("[Settings] Regenerating identity...");

    FS* fs = retOsGlobalPtr->hal().fs;

    // Delete identity file
    if (fs->exists("/reticulum/identity.priv")) {
        fs->remove("/reticulum/identity.priv");
        Serial.println("[Settings] Deleted identity.priv");
    }

    // Delete user info (full reset)
    if (fs->exists("/reticulum/userinfo.json")) {
        fs->remove("/reticulum/userinfo.json");
        Serial.println("[Settings] Deleted userinfo.json");
    }

    // Delete trusted servers (will be orphaned with new identity)
    if (fs->exists("/trusted_servers.json")) {
        fs->remove("/trusted_servers.json");
        Serial.println("[Settings] Deleted trusted_servers.json");
    }

    // Update button to show restarting
    if (label) {
        lv_label_set_text(label, LV_SYMBOL_REFRESH " Restarting...");
    }

    // Small delay so user sees the message
    delay(500);

    // Restart to regenerate identity
#ifdef ESP_PLATFORM
    ESP.restart();
#else
    Serial.println("[Settings] Emulator: Would restart here. Please restart manually.");
    // Reset state for emulator
    data->confirmPending = false;
    if (label) {
        lv_label_set_text(label, LV_SYMBOL_WARNING " Regenerate Identity");
    }
#endif
}

void Settings::drawDeviceIdentitySection() {
    drawSettingsSectionHeader(settings_column, "Device Identity");

    // Warning label
    lv_obj_t* warnLabel = lv_label_create(settings_column);
    lv_label_set_text(warnLabel, "This will delete your identity and\nall trusted server connections.");
    lv_obj_set_size(warnLabel, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_flag(warnLabel, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_set_style_text_color(warnLabel, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);
    lv_obj_set_style_pad_top(warnLabel, 3, LV_PART_MAIN);

    // Regenerate button - e-paper compatible styling
    lv_obj_t* btn = lv_btn_create(settings_column);
    lv_obj_set_size(btn, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_set_style_pad_all(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, retOsGlobalPtr->ui()->bg_color(), LV_PART_MAIN);

    lv_obj_t* btnLabel = lv_label_create(btn);
    lv_label_set_text(btnLabel, LV_SYMBOL_WARNING " Regenerate Identity");
    lv_obj_set_style_text_color(btnLabel, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);
    lv_obj_center(btnLabel);

    // Set up callback data
    identityBtnData.settings = this;
    identityBtnData.confirmPending = false;
    lv_obj_add_event_cb(btn, identityRegenCallback, LV_EVENT_CLICKED, &identityBtnData);
}