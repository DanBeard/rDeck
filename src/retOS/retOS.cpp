#include "retOS.h"
#include "../services/BaseService.h"
#include "../apps/BaseApp.h"
#include <lvgl.h>
#include "apps/Settings.h"


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
    // basic settile time
    delay(250);
    bool all_good;
    // wait until all the services are ready for us
    do { 
        all_good = true;
        for(auto runnable : retos->_services) {
            BaseService* service = (BaseService*) runnable;
            all_good = all_good && service->status() != STARTING;
        }
        delay(100);
    } while(!all_good);

    // load global settings
    // timezone
    JsonObject settings = Settings::getSettings(Settings::global_settings_section);
    const char* timezone = settings[Settings::timezone];
    if(timezone != nullptr && strnlen(timezone,2) > 0)  {
        retos->time.setPosixTimezone(timezone);
        Serial.print("Setting Timezone = ");
        Serial.println(timezone);
    } else {
        Serial.print("Can't set timezone to ");
        Serial.println(timezone);
    }

    // main loop
    while(true) {
        time_t tickTime = millis();

        // tick active app
        if(retos->_active_app != nullptr) {
            retos->_active_app->tick(tickTime);
        }

        static time_t lastSlowTickTime = 0;
        // slow ticks roughly every ~1-5 seconds  (|| for over-flow when mills() goes back to 0)
        if(tickTime - lastSlowTickTime > 1000 || lastSlowTickTime > tickTime){
            lastSlowTickTime = tickTime;
            // draw general UI elements like battery level and sensor status
            retos->_ui.slow_loop();

            // is it time to sleep?
            retos->maybeLightSleep();
        } 

        // loop UI/ timers
        uint32_t time_till_next = lv_timer_handler();
        lv_task_handler();
        //if(time_till_next == LV_NO_TIMER_READY) time_till_next = 5; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
        delay(min(time_till_next, (uint32_t) 5));    
    }
}

void _services_loop(void* _) {
     RetOS* retos = retOsGlobalPtr;

     //basic settle time
     delay(10);
     while(true) {
        const time_t tmillis = millis();
        for(auto service : retos->_services) {
            service->tick(tmillis);
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
    // this should return so call it last
    _hal.register_ui_task(_ui_loop);

}
void RetOS::launchApp(int8_t id){
    _ui.showLoadingScreen();

    // give the loading screen some frames before actuallying do the launching
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
    }, 90);
    
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

            _launcher->startApp(this);
            _ui.showAppScreen();
            _ui.hideBackButton();
            // launcher is NOT set to active app so it never really gets deleted. Just hidden.
     }, 25);
    
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

void RetOS::publishEvent(const Event& e) {
    // events should only come from services, so just run the service handlers in this current context.
    bool handled = false;
    for(auto runnable : _services) {
        BaseService* service = (BaseService*) runnable;
        EventStatus status = service->onEvent(e);
        if(status == HANDLED)  {
            handled = true;
            break;
        }
      }

      if(!handled && this->_active_app != nullptr) {
        // the app is in the UI task, so queue up it's handler using lvgl so its run in the UI task
            run_later([e, this](){
                if(this->_active_app != nullptr) { // double check since time has passed
                    this->_active_app->onEvent(e);
                }
            },1);
      }
      
    // always call the OS handlers, even if a service handled it    
    this->onEvent(e);
}

// the OS's own event handler
void RetOS::onEvent(const Event& e) {
    switch(e.type) {
        case RAW_GPS_TIME: {

        }
        break;
    }
}

void RetOS::initHardware(){

    if(_hal.screen) {
        // Screen
        _hal.screen->initScreen();
        _hal.screen->drawStartupScreen();
        _hal.screen->initLvgl();
    }

    if(_hal.keyboard) {
        _hal.keyboard->initKeyboard();
        _hal.keyboard->initLvgl();
    }
  
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

static uint64_t last_sleep = 0;
void RetOS::maybeLightSleep() {
    uint64_t last_action = 0;
    uint64_t now = millis();

    // loop through all the apps and services and hardware
    if(_active_app != nullptr) {
        if(_active_app->keepAwake()) return;
        last_action = _active_app->timeOfLastAction();
        if(last_action > now) last_action = 0; // rollover protection
    }

    // services
    for(auto runnable : _services) {
            BaseService* service = (BaseService*) runnable;
            uint64_t last_service_action = service->timeOfLastAction();
            if(last_service_action <= now && last_service_action > last_action) {
                last_action = last_service_action;
            }
    }

    // hardware
    uint64_t last_hardware_action = _hal.time_of_last_action();
    if(last_hardware_action<= now && last_hardware_action > last_action) {
        last_action = last_hardware_action;
    }

    //TODO Draw a sleep notification so the user knows we're asleep and will tap a button or something
    if(last_sleep == 0) {
        // on first boot give more time since user is probablt using it
        last_action += RETOS_LIGHT_SLEEP_AFTER_MS*2;
    }
    if(last_action + RETOS_LIGHT_SLEEP_AFTER_MS < now) { 
        last_sleep = now;
#ifndef SKIP_SLEEP
        _hal.light_sleep();
#endif
    }


}