#pragma once

#include <forward_list>
#include <functional>

#include "retHal/RetHal.h"
#include "retUI.h"
#include <ArduinoJson.h>

using namespace std;

class BaseApp;
class BaseService;

struct AppInfo {
    const char *name;
    const uint8_t id;
    const void* icon;
    function<BaseApp*()> factory ;
};

struct ServiceInfo {
    const uint8_t id;
    function<BaseService*()> factory ;
};

static uint8_t id_counter = 1; 

template<class T> AppInfo AppFactory(const char *name, const void* icon) {
    const uint8_t id = id_counter++;
    AppInfo result = {
        .name = name,
        .id = id,
        .icon = icon,
        .factory = [name, id](){return new T(name, id);}
    };
    return result;
};

 template<class T> ServiceInfo ServiceFactory() {
    const uint8_t id = id_counter++;
    ServiceInfo result = {
        .id = id,
        .factory = [id](){return new T(id);}
    };
    return result;
};


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
    void backToLauncher();

    // run the functor after ms milliseconds
    // THis function takes control of the functor and will delete the ptr after it's run
    void run_later(std::function<void()> func, uint32_t ms);

protected:
    BaseApp* _active_app = nullptr;
    BaseApp* _launcher;
    AppInfo _launcherInfo;

    RetHal _hal;
    RetUI _ui;
    forward_list<BaseService*> _services;

    forward_list<ServiceInfo> _serviceInfos;
    forward_list<AppInfo> _appInfos;

    uint32_t tick();
    void loop();

    void initHardware();

};

// we are a singleton so shove a pointer here in case we need to access it from C-style callbacks
extern RetOS* retOsGlobalPtr;