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

        virtual void start(RetOS* retos);
        virtual void tick();
        virtual void stop();

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