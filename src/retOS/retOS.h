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
    function<BaseApp*()> factory ;
};

struct ServiceInfo {
    const char *name;
    const uint8_t id;
    function<BaseService*()> factory ;
};

template<class T> constexpr AppInfo AppFactory(const char *name, const uint8_t id) {
    AppInfo result = {
        .name = name,
        .id = id,
        .factory = [name, id](){return new T(name, id);}
    };
    return result;
};

 template<class T> constexpr ServiceInfo ServiceFactory(const char *name, const uint8_t id) {
    ServiceInfo result = {
        .name = name,
        .id = id,
        .factory = [name,id](){return new T(name, id);}
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
    void launchApp(int8_t id, JsonDocument& args);
    void backToLauncher();

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