#pragma once
#include "BaseApp.h"
#include "ArduinoJson.h"


typedef std::function<void(lv_event_t *)> FunctorCallback;

class Settings : public BaseApp {
    public:
        // inherit default ctor
        using BaseApp::BaseApp;

        static constexpr const char* settingsFile = "/settings.json";
        static constexpr const char* global_settings_section = "settings";
        static constexpr const char* timezone = "timezone";
        static constexpr const char* epoch = "epoch";

        virtual void start(RetOS* retos);
        virtual void tick(const time_t tickMillis);
        virtual void stop();

        // warning DO NOT KEEP THESE JSON OBJECTS AROUND. Deserialize and move on
        // THey will become invalid if the doc gets clear or cleaned up after serialization
        static JsonObject getSettings(const char* section);
        static void saveSettings();

        // drawing helpers
        lv_obj_t* drawSettingsTextInputRow(lv_obj_t* container, const char* title, const char* value, FunctorCallback *callback);
        static void drawSettingsSectionHeader(lv_obj_t* container, const char* title);

    protected:
        lv_obj_t * settings_column;

        JsonObject _settings;

        void drawScreen();
        void drawTimeDateSection();


};