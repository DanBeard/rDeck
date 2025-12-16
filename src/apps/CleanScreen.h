#pragma once
#include "BaseApp.h"

class CleanScreen : public BaseApp {
    public:
    // inherit default ctor
    using BaseApp::BaseApp;

    virtual void start(RetOS* retos) override;
    virtual void tick(const unsigned long tickMillis) override;
    virtual void stop() override;


};