#include "BaseApp.h"

class RetUI;

class Launcher : public BaseApp {
public:
    // inherit default ctor
    using BaseApp::BaseApp;

    virtual void start(RetOS* retos);
    virtual void tick();
    virtual void stop();

    virtual EventStatus onEvent(Event& event);

protected:
    const RetUI* _ui;

    void menu_btn_create(lv_obj_t *parent, AppInfo& appInfo, int x, int y);

};