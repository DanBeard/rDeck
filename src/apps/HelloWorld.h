#pragma once
#include "BaseApp.h"

class HelloWorld : public BaseApp {
    public:
    // inherit default ctor
    using BaseApp::BaseApp;

    virtual void start(RetOS* retos) override;
    virtual void tick(const time_t tickMillis) override;
    virtual void stop() override;


};