#pragma once

#include <forward_list>
#include <functional>

#include "retHal/RetHal.h"
#include "retUI.h"
#include "Events.h"
#include <ArduinoJson.h>
#include "./retosUtils/TimeHelper.h"
#include "./RetRunnable.h"

#define RETOS_LIGHT_SLEEP_AFTER_MS (1000*1000)
#define SKIP_SLEEP

using namespace std;

class BaseApp;
class BaseService;
class Settings;

struct AppInfo {
    const char *name;
    const uint8_t id;
    const void* icon;
    function<BaseApp*()> factory ;
};

struct ServiceInfo {
    const uint8_t id;
    function<BaseService*()> factory ;
    function<boolean(lv_obj_t * column, Settings* settings)> drawSettings;
    function<void()> applySettings;
};

inline uint8_t id_counter = 1;

// static so id should always be the same for the same template
template<class T> AppInfo AppFactory(const char *name, const void* icon) {
    static const uint8_t id = id_counter++;
    static AppInfo result = {
        .name = name,
        .id = id,
        .icon = icon,
        .factory = [name](){return new T(name, id);}
    };
    return result;
};

// static so id should always be the same for the same template
 template<class T> ServiceInfo ServiceFactory() {
    static uint8_t id = id_counter++;
    static ServiceInfo result = {
        .id = id,
        .factory = [](){return new T(id);},
        // proxy out the static settings saccessors
        .drawSettings = &(T::drawSettings),
        .applySettings= &(T::applySettings)
    };
    return result;
};


class DateTimeService;

class RetOS {

public:
    RetOS(RetHal hal, forward_list<ServiceInfo> services, forward_list<AppInfo> apps,  AppInfo launcherFactory);

    // From here, RetOS takes over control 
    void start();
    const RetHal hal() const;
    RetUI* ui();
    const forward_list<ServiceInfo>& serviceInfo() const;
    const forward_list<AppInfo>& appInfo() const;

    void launchApp(int8_t id);
    BaseApp* activeApp() const {return _active_app;};
    void backToLauncher();

    // expected to be called in the services task ONLY!!!
    void publishEvent(const Event& e);

    // run the functor after ms milliseconds
    void run_later(std::function<void()> func, uint32_t ms);

    

    TimeHelper time;

protected:
    BaseApp* _active_app = nullptr;
    BaseApp* _launcher;
    AppInfo _launcherInfo;

    RetHal _hal;
    RetUI _ui;
    forward_list<RetRunnable*> _services;

    forward_list<ServiceInfo> _serviceInfos;
    forward_list<AppInfo> _appInfos;

    friend void _ui_loop(void *);
    friend void _services_loop(void *);

    void onEvent(const Event& e);

    void initHardware();
    void maybeLightSleep();

    
public:

    template<class T> T* fetchService() {
        ServiceInfo info = ServiceFactory<T>();
        const uint8_t sId = info.id;

        for(RetRunnable* service : _services) {
            if(service->id() == sId) {
                return (T*) service;
            }
        }
        // no such service O.o
        return nullptr;

     };

};

// we are a singleton so shove a pointer here in case we need to access it from C-style callbacks
extern RetOS* retOsGlobalPtr;