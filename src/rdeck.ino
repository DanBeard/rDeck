#include "rdeck.h"
#include <Arduino.h>
#include "retOS/retOS.h"
#include "apps/apps.h"
#include "services/services.h"
#include "assets/assets.h"

// board config
#include "boards/board.h"

forward_list<AppInfo> apps= {
     AppFactory<ClockApp>("Clock", &img_clock),
     AppFactory<NotesApp>("Notes", &img_notes),
     AppFactory<UChat>("uChat", &img_chat),
     AppFactory<WebSearch>("Search", &img_wifi),
     AppFactory<CleanScreen>("Clean", &img_gear),
     AppFactory<Settings>("Settings", &img_gear)
};

forward_list<ServiceInfo> services = {
     ServiceFactory<GPSService>(),
     ServiceFactory<RnsService>(),
     ServiceFactory<TimeService>(),
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