#pragma once
#include "BaseApp.h"

class HelloWorld : public BaseApp {
    public:
    // inherit default ctor
    using BaseApp::BaseApp;

    virtual void start(JsonDocument& args, RetOS* retos);
    virtual void tick();
    virtual void stop();

    virtual EventStatus onEvent(Event& event);

};