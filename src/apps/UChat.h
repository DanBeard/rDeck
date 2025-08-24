#pragma once
#include "BaseApp.h"

class RnsService;

class UChat : public BaseApp {
    public:
    // inherit default ctor
        using BaseApp::BaseApp;

        virtual void start(RetOS* retos) override;
        virtual void tick(const time_t tickMillis) override;
        virtual void stop() override;

    protected:
            RnsService * _rns_service;

            void renderMainMenu();
            
            lv_obj_t * tabview;
            lv_obj_t * msgview;
            lv_obj_t * announceview;
            lv_obj_t * statusview;


};