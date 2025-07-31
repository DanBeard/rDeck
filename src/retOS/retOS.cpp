#include "retOS.h"
#include "../services/BaseService.h"
#include "../apps/BaseApp.h"
#include <lvgl.h>
#include "defines.h"
#include <GxEPD2_BW.h>
#include <TouchDrvCSTXXX.hpp>
#include <Fonts/FreeMonoBold9pt7b.h>

using namespace std;

RetOS::RetOS(forward_list<ServiceFactory> services, forward_list<AppFactory> apps, AppFactory launcherFactory) :
_serviceFactories(services),
_appFactories(apps),
_launcherFactory(launcherFactory) {


}

void RetOS::start(){
    // no active app yet
    _active_app = nullptr;

    drawStartupScreen();

    // construct services
    for(auto sFactory : _serviceFactories) {
        BaseService* service = sFactory();
        _services.push_front(service);
        service->start(this);
    }

    //TODO: startup screen?

    // wait for all the services  to finish starting up
    bool all_good = true;
    do {
        all_good = true;
        for(auto service : _services) {
            all_good = all_good && service->status() != STARTING;
        }
        if(!all_good) {
            tick();
            delay(2);
        }
    } while(!all_good);
    

    // construct the launcher

    Serial.println("Starting launcher app.....");
    _launcher = _launcherFactory();

    // will not return from here
    loop();

}

uint32_t RetOS::tick() {
            // loop services
            for(auto service : _services) {
                service->loop();
            }
            // loop apps
            if(_active_app != nullptr) {
                _active_app->loop();
            }
            // loop UI/ timers
            uint32_t time_till_next = lv_timer_handler();
            return time_till_next;
}

void RetOS::loop() {
    while(true) {
        uint32_t time_till_next = tick();
        if(time_till_next == LV_NO_TIMER_READY) time_till_next = 100; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
        delay(time_till_next);
    }
}

TouchDrvCSTXXX touch;
GxEPD2_BW<GxEPD2_310_GDEQ031T10, GxEPD2_310_GDEQ031T10::HEIGHT> display(GxEPD2_310_GDEQ031T10(BOARD_EPD_CS, BOARD_EPD_DC, BOARD_EPD_RST, BOARD_EPD_BUSY)); // GDEQ031T10 240x320, UC8253, (no inking, backside mark KEGMO 3100)
const char WelcomeMsg[] = "RetOS";

#define BOARD_SPI_CS    34
#define BOARD_SPI_DC    35
#define BOARD_SPI_RST  -1
#define BOARD_SPI_BUSY 37
#define BOARD_SPI_SCK  36
#define BOARD_SPI_MOSI 33


void RetOS::drawStartupScreen(){

    Serial.println("Drawing startup screen.....");
    SPI.begin(BOARD_SPI_SCK, -1, BOARD_SPI_MOSI, BOARD_SPI_CS);
    // SPI.begin(BOARD_SPI_SCK, -1, BOARD_SPI_MOSI, BOARD_EPD_CS);
    display.init(115200, true, 2, false);
    //Serial.println("helloWorld");
    display.setRotation(0);
    display.setFont(&FreeMonoBold9pt7b);
    if (display.epd2.WIDTH < 104) display.setFont(0);
    display.setTextColor(GxEPD_BLACK);
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds(WelcomeMsg, 0, 0, &tbx, &tby, &tbw, &tbh);
    // center bounding box by transposition of origin:
    uint16_t x = ((display.width() - tbw) / 2) - tbx;
    uint16_t y = ((display.height() - tbh) / 2) - tby;
    display.setFullWindow();
    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(x, y);
        display.print(WelcomeMsg);
    }
    while (display.nextPage());
    display.hibernate();
}