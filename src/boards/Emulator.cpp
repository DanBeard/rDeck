#ifdef RET_PLATFORM_EMU
#include <Arduino.h>

#include <TouchDrvCSTXXX.hpp>

#include "TDeckPro.h"
#include "retHal/Screen/TDeckProScreen.h"
#include "retHal/Keyboard/TDeckProKeyboard.h"
#include <SD.h>
#include "retHal/Battery/TDeckProBattery.h"
#include "retHal/GPS/TDeckProGPS.h"
#include "retHal/Lora/TDeckProLora.h"

TDeckProScreen screen;
TDeckProKeyboard kb;
TDeckProBattery bat;
TDeckProGPS gps;
TDeckProLora lora;

TaskHandle_t uiTask;
TaskHandle_t servicesTask;


void register_ui_task(RetTask task) {
   xTaskCreatePinnedToCore(task, "UITask", 10000, NULL, 1, &uiTask, 1);
}

void register_service_task(RetTask task) {
    xTaskCreatePinnedToCore(task, "ServiceTask", 10000, NULL, 2, &servicesTask, 1);
}

uint64_t time_of_last_action = 0;
uint64_t time_of_last_action_getter() {
    return time_of_last_action;
}

const gpio_num_t buttonPin1 = GPIO_NUM_0; // GPIO for pushbutton 1
const gpio_num_t buttonPin2 = GPIO_NUM_27; // GPIO for pushbutton 2

int wakeup_gpio; // Variable to store the GPIO that caused wake-up

// ISR for buttonPin1
void IRAM_ATTR handleInterrupt1() {
    wakeup_gpio = buttonPin1;
}

// ISR for buttonPin2
void IRAM_ATTR handleInterrupt2() {
    wakeup_gpio = buttonPin2;
}


void light_sleep() {
    Serial.println("Going to sleep.");
    //touchSleepWakeUpEnable(BOARD_TOUCH_RST, 100);
    //gpio_num_t pwr_key = GPIO_NUM_1;
    //gpio_wakeup_enable(buttonPin1, GPIO_INTR_HIGH_LEVEL); // Trigger wake-up on high level
    //gpio_wakeup_enable(buttonPin2, GPIO_INTR_HIGH_LEVEL); // Trigger wake-up on high level
    // Enable wake-up by timer
    const uint64_t sleepTime = 1000000*5;  // Sleep duration in microseconds (5 seconds)
    esp_err_t result = esp_sleep_enable_timer_wakeup(sleepTime);

    if (result == ESP_OK) {
        Serial.println("Timer Wake-Up set successfully as wake-up source.");
    } else {
        Serial.println("Failed to set Timer Wake-Up as wake-up source.");
    }
    delay(1);
    esp_light_sleep_start();
    Serial.println("Woke from sleep!");
    //Serial.println(wakeup_gpio);
    //Serial.println("^^^^^^");
}


RetHal tDeckProHal = {
    .screen = &screen,
    .fs     = nullptr,
    .keyboard = nullptr,
    .battery = nullptr,
    .gps = nullptr,
    .lora = nullptr,

    .register_ui_task = (register_ui_task),
    .register_service_task = (register_service_task), 
    .time_of_last_action = (time_of_last_action_getter),
    .light_sleep = (light_sleep),
};



void tDeckBoardInit() {

}
#endif