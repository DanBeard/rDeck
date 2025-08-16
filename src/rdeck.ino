#include "rdeck.h"
#include <Arduino.h>
#include "retOS/retOS.h"
#include "apps/apps.h"
#include "services/services.h"
#include "assets/assets.h"

// board config
#include "boards/board.h"

forward_list<AppInfo> apps= {
     AppFactory<HelloWorld>("HW", &img_setting), 
     AppFactory<NotesApp>("Notes", &img_test),  
   AppFactory<Settings>("Settings", &img_setting)
};

forward_list<ServiceInfo> services = {
     ServiceFactory<GPSService>(),
     ServiceFactory<RnsService>(), 
     };

// main app that launches other apps
AppInfo launcherFactory = AppFactory<Launcher>("Launcher", nullptr);


RetOS retos(BOARD_HAL, services, apps, launcherFactory);

void setup() {
     BOARD_INIT
     retos.start();

}


void loop() {
     
}