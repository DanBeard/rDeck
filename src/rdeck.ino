#include <Arduino.h>
#include "rdeck.h"
#include "retOS/retOS.h"
#include "apps/apps.h"
#include "services/services.h"
#include "assets/assets.h"

// board config
 #include "boards/TDeckPro.h"
// #include "utilities.h"
// #include <TouchDrvCSTXXX.hpp>
// TouchDrvCSTXXX touch;

forward_list<AppInfo> apps= {AppFactory<HelloWorld>("HW", &img_setting), AppFactory<NotesApp>("Notes", &img_test)};
forward_list<ServiceInfo> services = {
      ServiceFactory<GPSService>(),
      ServiceFactory<RnsService>(), 
     };

// main app that launches other apps
AppInfo launcherFactory = AppFactory<Launcher>("Launcher", nullptr);


RetOS retos(BOARD_HAL, services, apps, launcherFactory);

void setup() {
     BOARD_INIT


//     Serial.begin(115200);
    
//     touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_INT);
//     bool hasTouch = touch.begin(Wire, BOARD_I2C_ADDR_TOUCH, BOARD_TOUCH_SDA, BOARD_TOUCH_SCL);
//     if (!hasTouch) {
//         Serial.println("Failed to find Capacitive Touch !");
//     } else {
//         Serial.println("Find Capacitive Touch");
//     }

}


void loop() {
    // retos takes control from here
    // will not return.
     retos.start();
}