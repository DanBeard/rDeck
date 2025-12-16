#pragma once
#include "BaseApp.h"

class NotesApp : public BaseApp {
    public:
    // inherit default ctor
    using BaseApp::BaseApp;

    const char* notesPath = "/notes.txt";
    lv_obj_t * ta;

    virtual void start(RetOS* retos);
    virtual void tick(const unsigned long tickMillis);
    virtual void stop();


};