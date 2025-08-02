#pragma once
#include "BaseService.h"

class LoraService: public BaseService {
    
    using BaseService::BaseService;

public:

    virtual void start(RetOS* retos); // called after construction once the OS is ready to launch services
    virtual void loop(); // called periodically by OS so you can do work. TIme varies by sleep and power level.
    virtual EventStatus onEvent(Event& event);

    bool isValid = false;

protected:
    void updateIcon(bool status);

};