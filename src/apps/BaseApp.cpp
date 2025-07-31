#include "BaseApp.h"
#include <string.h>

BaseApp::BaseApp(const char *name, const uint8_t id) : 
 _id(id), _args(DynamicJsonDocument(0)) {
    strncpy(_name, name, RETOS_MAX_APP_NAME_SIZE + 1);
}

void BaseApp::startApp(JsonDocument& args, RetOS* retos) {
    _args = args;
    _retos = retos;

    retos->ui()->clear_app_screen();
    screen = retos->ui()->app_screen();
    this->start(args, retos);
}

const uint8_t BaseApp::id() const
{
    return _id;
}
