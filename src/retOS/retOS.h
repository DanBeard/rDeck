#pragma once

#include <forward_list>
#include <functional>

using namespace std;

class BaseApp;
class BaseService;


typedef function<BaseApp*()> AppFactory;
typedef function<BaseService*()> ServiceFactory;

class RetOS {

public:
    RetOS(forward_list<ServiceFactory> services, forward_list<AppFactory> apps,  AppFactory launcherFactory);

    // From here, RetOS takes over control 
    void start();

protected:
    BaseApp* _active_app = nullptr;
    BaseApp* _launcher;
    AppFactory _launcherFactory;

    forward_list<BaseService*> _services;

    forward_list<ServiceFactory> _serviceFactories;
    forward_list<AppFactory> _appFactories;

    uint32_t tick();
    void loop();

    void drawStartupScreen();

};