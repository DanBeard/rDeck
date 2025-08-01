#pragma once
#include "BaseApp.h"

class NotesApp : public BaseApp {
    public:
    // inherit default ctor
    using BaseApp::BaseApp;

    const char* notesPath = "/notes.txt";
    lv_obj_t * ta;

    virtual void start(JsonDocument& args, RetOS* retos);
    virtual void loop();
    virtual void stop();

    virtual EventStatus onEvent(Event& event);

};