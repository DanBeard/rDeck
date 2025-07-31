#include "rdeck.h"
#include "retOS/retOS.h"
#include "apps/Launcher.h"

forward_list<AppFactory> apps={};
forward_list<ServiceFactory> services = {};
AppFactory launcherFactory = [](){return new Launcher("Launcher", 0);};

RetOS retos(services, apps, launcherFactory);

void setup() {
     Serial.begin(115200);
        Serial.println("STARTING.....");

        
}


void loop() {
    // retos takes control from here
    // will not return.
     retos.start();
}