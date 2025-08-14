#include "BaseApp.h"
#include <string.h>

BaseApp::BaseApp(const char *name, const uint8_t id) : 
 _id(id) {
    strncpy(_name, name, RETOS_MAX_APP_NAME_SIZE + 1);
}

void BaseApp::startApp( RetOS* retos) {
    _retos = retos;

    retos->ui()->clear_app_screen();
    screen = retos->ui()->app_screen();
    this->start(retos);
}

const uint8_t BaseApp::id() const
{
    return _id;
}

EventStatus BaseApp::onEvent(const Event& event) {
    return IGNORED;
}
