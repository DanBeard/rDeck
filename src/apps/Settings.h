#pragma once
#include "BaseApp.h"

class Settings : public BaseApp {
    public:
    // inherit default ctor
    using BaseApp::BaseApp;

    virtual void start(RetOS* retos);
    virtual void tick();
    virtual void stop();


    protected:
        lv_obj_t * settings_column;

        void drawScreen();
        void drawTimeDateSection();


};