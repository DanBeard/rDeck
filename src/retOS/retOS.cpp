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
            service->startService(this);
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
        _ui.hideBackButton();
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
    _ui.showLoadingScreen();
    // TODO: Lots of Heap anc copy shenanigans here. But its only when launching an app
    // Profile and see if we need to clean it up. Do we even need the args anymore?
    // Could save space and time by removing them!!

    // give the loading screen one frame before actuallying do the launching
    DynamicJsonDocument *heap_args = new DynamicJsonDocument(args.capacity());
    run_later([this, id, heap_args]() {
        // clean up current app
    if(_active_app) {
        _active_app->stop();
        delete _active_app;
        _active_app = nullptr;
    }

    for(AppInfo app : _appInfos){
        if(app.id == id) {
            _active_app = app.factory();
            _active_app->startApp(*heap_args, this);
             _ui.showAppScreen();
             _ui.showBackButton();
            delete heap_args;
            return;
        }
    }


    // oops, we didn't find it? just go back to launcher then
    backToLauncher();
    }, 16);
    
}
void RetOS::backToLauncher(){
    _ui.showLoadingScreen();
     // give the loading screen one frame before actuallying do the launching
     run_later([this](){
            // clean up current app
            if(_active_app) {
                _active_app->stop();
                delete _active_app;
                _active_app = nullptr;
            }

            StaticJsonDocument<0> args;
            _launcher->startApp(args, this);
            _ui.showAppScreen();
            _ui.hideBackButton();
            // launcher is NOT set to active app so it never really gets deleted. Just hidden.
     }, 16);
    
}

uint32_t RetOS::tick() {
            // loop services
            for(auto service : _services) {
                service->tick();
            }
            // loop apps
            if(_active_app != nullptr) {
                _active_app->tick();
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
    uint32_t tickCount = 0;
    while(true) {
        // slow ticks roughly every 30 seconds?
        if(tickCount % 10 == 0){
            _ui.slow_loop();
        } 

        uint32_t time_till_next = tick();
        if(time_till_next == LV_NO_TIMER_READY) time_till_next = 16; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
        delay(time_till_next);       
        tickCount++;
    }
}

static void delay_timer(lv_timer_t * timer) {
    std::function<void()> *func = (std::function<void()> *)timer->user_data;
    (*func)(); // call it
    delete func; //clean it up
}

//TouchDrvCSTXXX touch;
void  RetOS::run_later(std::function<void()> func, uint32_t ms) {
    std::function<void()> *heap_func = new std::function<void()>(func);
    lv_timer_t * timer = lv_timer_create(delay_timer, ms,  heap_func);
    lv_timer_set_repeat_count(timer, 1);
}

void RetOS::initHardware(){
    // Screen
    _hal.screen->initScreen();
    _hal.screen->drawStartupScreen();
    _hal.screen->initLvgl();

    _hal.keyboard->initKeyboard();
    _hal.keyboard->initLvgl();  
    
    if(_hal.battery) {
        _hal.battery->initBattery();
    }

    if(_hal.gps) {
        _hal.gps->initGPS();
    }

    if(_hal.lora) {
        _hal.lora->initLora();
    }
}