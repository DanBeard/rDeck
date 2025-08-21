#pragma once
#include "BaseApp.h"

class NotesApp : public BaseApp {
    public:
    // inherit default ctor
    using BaseApp::BaseApp;

    const char* notesPath = "/notes.txt";
    lv_obj_t * ta;

    virtual void start(RetOS* retos);
    virtual void tick(const time_t tickMillis);
    virtual void stop();


};