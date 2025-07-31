#include "BaseApp.h"

class Launcher : public BaseApp {
public:
    // inherit default ctor
    using BaseApp::BaseApp;

    virtual void start(JsonDocument& args, RetOS* retos);
    virtual void loop();
    virtual void stop();

    virtual EventStatus onEvent(Event& event);

};