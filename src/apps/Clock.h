#pragma once
#include "BaseApp.h"
#include "sys/time.h"

class ClockApp : public BaseApp {
    public:
        // inherit default ctor
        using BaseApp::BaseApp;

        static const long update_every_sec = 10;

        virtual void start(RetOS* retos);
        virtual void tick();
        virtual void stop();

        void updateTime();

    protected:
        lv_obj_t *_time_lbl = nullptr;
        lv_obj_t *_date_lbl = nullptr;
        time_t _last_time_grabbed = 0;
        tm _last_time_info = {0};
};