#include <Arduino.h>

#include <TouchDrvCSTXXX.hpp>

#include "TDeckPro.h"
#include "retHal/Screen/TDeckProScreen.h"
#include "retHal/Keyboard/TDeckProKeyboard.h"
#include <SD.h>
#include "retHal/Battery/TDeckProBattery.h"
#include "retHal/GPS/TDeckProGPS.h"

TDeckProScreen screen;
TDeckProKeyboard kb;
TDeckProBattery bat;
TDeckProGPS gps;

RetHal tDeckProHal = {
    .screen = &screen,
    .fs     = &SD,
    .keyboard = &kb,
    .battery = &bat,
    .gps = &gps
};

// board init
TouchDrvCSTXXX tDeckProTouch;

void tDeckBoardInit() {
     // LORA、SD、EPD use the same SPI, in order to avoid mutual influence;
    // before powering on, all CS signals should be pulled high and in an unselected state;
    pinMode(BOARD_EPD_CS, OUTPUT); 
    digitalWrite(BOARD_EPD_CS, HIGH);
    pinMode(BOARD_SD_CS, OUTPUT); 
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT); 
    digitalWrite(BOARD_LORA_CS, HIGH);

    Serial.begin(115200);

    // IO
    pinMode(BOARD_KEYBOARD_LED, OUTPUT);
    pinMode(BOARD_MOTOR_PIN, OUTPUT);
    pinMode(BOARD_6609_EN, OUTPUT);         // enable 7682 module
    pinMode(BOARD_LORA_EN, OUTPUT);         // enable LORA module
    pinMode(BOARD_GPS_EN, OUTPUT);          // enable GPS module
    pinMode(BOARD_1V8_EN, OUTPUT);          // enable gyroscope module
    pinMode(BOARD_A7682E_PWRKEY, OUTPUT); 
    digitalWrite(BOARD_KEYBOARD_LED, LOW);
    digitalWrite(BOARD_MOTOR_PIN, LOW);
    digitalWrite(BOARD_6609_EN, HIGH);
    digitalWrite(BOARD_LORA_EN, HIGH);
    digitalWrite(BOARD_GPS_EN, HIGH);
    digitalWrite(BOARD_1V8_EN, HIGH);
    digitalWrite(BOARD_A7682E_PWRKEY, HIGH);

    delay(100);
    // i2c devices
    byte error, address;
    int nDevices = 0;
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    Serial.printf(" ------------- I2C ------------- \n");
    for(address = 0x01; address < 0x7F; address++){
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if(error == 0){ // 0: success.
            nDevices++;
            if(address == BOARD_I2C_ADDR_TOUCH){
                // flag_Touch_init = true;
                Serial.printf("[0x%x] TOUCH find!\n", address);
            } else if (address == BOARD_I2C_ADDR_LTR_553ALS) {
                Serial.printf("[0x%x] LTR_553ALS find!\n", address);
            } else if (address == BOARD_I2C_ADDR_GYROSCOPDE) {
                Serial.printf("[0x%x] GYROSCOPDE find!\n", address);
            } else if (address == BOARD_I2C_ADDR_KEYBOARD) {
                Serial.printf("[0x%x] KEYBOARD find!\n", address);
            } else if (address == BOARD_I2C_ADDR_BQ27220) {
                Serial.printf("[0x%x] BQ27220 find!\n", address);
            } else if (address == BOARD_I2C_ADDR_BQ25896) {
                Serial.printf("[0x%x] BQ25896 find!\n", address);
            }
        }
    }


    Serial.println(" ------------- PERIPHERALS ------------- ");
    delay(1000);
    // SPI
    SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);

    // init peripheral
    tDeckProTouch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_INT);
    bool good =tDeckProTouch.begin(Wire, BOARD_I2C_ADDR_TOUCH, BOARD_TOUCH_SDA, BOARD_TOUCH_SCL);
    if(good) Serial.println("Touch begin true");
    else Serial.println("Touch begin false");

    // SD card
    if(!SD.begin(BOARD_SD_CS)){
        Serial.println("[SD CARD] Card Mount Failed");
    }


    // TODO
    /*
    peri_init_st[E_PERI_INK_SCREEN] = ink_screen_init();
    peri_init_st[E_PERI_LORA]       = lora_init();
    peri_init_st[E_PERI_TOUCH]      = touch.begin(Wire, BOARD_I2C_ADDR_TOUCH, BOARD_TOUCH_SDA, BOARD_TOUCH_SCL);
    peri_init_st[E_PERI_KYEPAD]     = keypad_init(BOARD_I2C_ADDR_KEYBOARD);
    peri_init_st[E_PERI_BQ25896]    = bq25896_init();
    peri_init_st[E_PERI_BQ27220]    = bq27220_init();
    peri_init_st[E_PERI_SD]         = sd_care_init();
    peri_init_st[E_PERI_GPS]        = gps_init();
    peri_init_st[E_PERI_BHI260AP]   = BHI260AP_init();
    peri_init_st[E_PERI_LTR_553ALS] = LTR553_init();
    peri_init_st[E_PERI_A7682E]     = A7682E_init();

    if(peri_init_st[E_PERI_A7682E] == false)
    {
        peri_init_st[E_PERI_PCM5102A] = pcm5102a_init();
    }

    lvgl_init();

    ui_deckpro_entry();

    disp_full_refr();
    */
}