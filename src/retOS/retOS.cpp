#include "retOS.h"
#include "../services/BaseService.h"
#include "../apps/BaseApp.h"
#include <lvgl.h>

using namespace std;
RetOS* retOsGlobalPtr;

RetOS::RetOS(RetHal hal,forward_list<ServiceInfo> services, forward_list<AppInfo> apps, AppInfo launcher) :
_hal(hal),
_serviceInfos(services),
_appInfos(apps),
_launcherInfo(launcher),
_ui(this) {
    retOsGlobalPtr = this;
}

const RetHal RetOS::hal() const {
    return _hal;
}

RetUI* RetOS::ui() {
    return &_ui;
}

const forward_list<ServiceInfo>& RetOS::serviceInfo() const {
    return _serviceInfos;
}

const forward_list<AppInfo>& RetOS::appInfo() const {
    return _appInfos;
}


void RetOS::start(){
    { 
        // no active app yet
        _active_app = nullptr;

        initHardware();

        // construct services
        for(auto sInfo : _serviceInfos) {
            BaseService* service = sInfo.factory();
            _services.push_front(service);
            service->start(this);
        }

        // wait for all the services  to finish starting up
        bool all_good = true;
        do {
            all_good = true;
            for(auto service : _services) {
                all_good = all_good && service->status() != STARTING;
            }
            if(!all_good) {
                tick();
                delay(1);
            }
        } while(!all_good);
        

        // init the UI
        _ui.init();

        // construct and start the launcher
        Serial.println("Starting launcher app.....");
        _launcher = _launcherInfo.factory();
        StaticJsonDocument<0> emptyDoc;
        _launcher->startApp(emptyDoc, this);
    }

    // will not return from here
    loop();

}
void RetOS::launchApp(int8_t id){
    StaticJsonDocument<0> args;
    return launchApp(id, args);
}
void RetOS::launchApp(int8_t id, JsonDocument& args){
    // clean up current app
    if(_active_app) {
        _active_app->stop();
        delete _active_app;
        _active_app = nullptr;
    }

    for(AppInfo app : _appInfos){
        if(app.id == id) {
            _active_app = app.factory();
            _active_app->startApp(args, this);
            return;
        }
    }

    // oops, we didn't find it? just go back to launcher then
    backToLauncher();
}
void RetOS::backToLauncher(){
    // clean up current app
    if(_active_app) {
        _active_app->stop();
        delete _active_app;
        _active_app = nullptr;
    }

    StaticJsonDocument<0> args;
    _launcher->startApp(args, this);
    // launcher is NOT set to active app so it never really gets deleted. Just hidden.
}

uint32_t RetOS::tick() {
            // loop services
            for(auto service : _services) {
                service->loop();
            }
            // loop apps
            if(_active_app != nullptr) {
                _active_app->loop();
            }
            // loop UI/ timers
            uint32_t time_till_next = lv_timer_handler();
            lv_task_handler();
            //TODO REMOVE ME
            _hal.screen->fullRefresh();
            //Serial.printf("tick %u -- ", time_till_next);
            return time_till_next;
}

void RetOS::loop() {
    while(true) {
        uint32_t time_till_next = tick();
        if(time_till_next == LV_NO_TIMER_READY) time_till_next = 250; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
        delay(time_till_next);
    }
}

//TouchDrvCSTXXX touch;

void RetOS::initHardware(){
    // Screen
    _hal.screen->initScreen();
    _hal.screen->drawStartupScreen();
    _hal.screen->initLvgl();
}