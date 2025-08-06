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

void _ui_loop(void* _) {
    RetOS* retos = retOsGlobalPtr;
    delay(250);
    bool all_good;
    do { 
        all_good = true;
        for(auto service : retos->_services) {
            all_good = all_good && service->status() != STARTING;
        }
        delay(5);
    } while(!all_good);


    uint32_t tickCount = 0;
    while(true) {
        // slow ticks roughly every 30+ seconds?
        if(tickCount % 10 == 0){
            retos->_ui.slow_loop();
        } 

        // loop apps
        if(retos->_active_app != nullptr) {
            retos->_active_app->tick();
        }
        // loop UI/ timers
        uint32_t time_till_next = lv_timer_handler();
        lv_task_handler();
        //if(time_till_next == LV_NO_TIMER_READY) time_till_next = 5; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
        delay(min(time_till_next, (uint32_t) 20));    
        tickCount++;
    }
}

void _services_loop(void* _) {
     RetOS* retos = retOsGlobalPtr;

     delay(10);
     while(true) {
        for(auto service : retos->_services) {
            service->tick();
      }
      delay(1);   
     }
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
        
        // init the UI
        _ui.init();

        // construct and start the launcher
        Serial.println("Starting launcher app.....");
        _ui.hideBackButton();
        _launcher = _launcherInfo.factory();
        _launcher->startApp(this);
    }

    // let's boot up!
    _hal.register_service_task(_services_loop);
    _hal.register_ui_task(_ui_loop);

}
void RetOS::launchApp(int8_t id){
    _ui.showLoadingScreen();
    // TODO: Lots of Heap anc copy shenanigans here. But its only when launching an app
    // Profile and see if we need to clean it up. Do we even need the args anymore?
    // Could save space and time by removing them!!

    // give the loading screen one frame before actuallying do the launching
    run_later([this, id]() {
        // clean up current app
    if(_active_app) {
        _active_app->stop();
        delete _active_app;
        _active_app = nullptr;
    }

    for(AppInfo app : _appInfos){
        if(app.id == id) {
            _active_app = app.factory();
            _active_app->startApp(this);
             _ui.showAppScreen();
             _ui.showBackButton();
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
            _launcher->startApp(this);
            _ui.showAppScreen();
            _ui.hideBackButton();
            // launcher is NOT set to active app so it never really gets deleted. Just hidden.
     }, 16);
    
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