#pragma once
#include "BaseApp.h"
#include "ArduinoJson.h"

class Settings : public BaseApp {
    public:
    // inherit default ctor
    using BaseApp::BaseApp;

    static constexpr const char* settingsFile = "/settings.json";

    virtual void start(RetOS* retos);
    virtual void tick();
    virtual void stop();

    static JsonObject getSettings(const char* section);
    static void saveSettings();

    protected:
        lv_obj_t * settings_column;

        JsonObject _settings;

        void drawScreen();
        void drawTimeDateSection();


};