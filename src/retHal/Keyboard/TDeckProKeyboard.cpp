#include "TDeckProKeyboard.h"
#include "boards/TDeckPro.h"
#include <Adafruit_TCA8418.h>
#include "lvgl.h"

#include "retOS/retOS.h"

#define KEYPAD_ROWS 4
#define KEYPAD_COLS 10
#define KEYPAD_PRESS_VAL_MIN   129
#define KEYPAD_PRESS_VAL_MAX   163
#define KEYPAD_RELEASE_VAL_MIN 1
#define KEYPAD_RELEASE_VAL_MAX 35

const char keymap[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', LV_KEY_BACKSPACE}, //backspace
    {'2', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '$', LV_KEY_ENTER},  //enter/LF
    {' ', ' ', ' ', ' ', ' ', '-', '*', ' ', '0', 0x0F},
};

Adafruit_TCA8418 keypad; 

// init the hardware. Called once on boot
/*virtual*/ void TDeckProKeyboard::initKeyboard() {
    int addr = BOARD_I2C_ADDR_KEYBOARD;
     if(!i2cIsInit(0)){
        Wire.begin(BOARD_KEYBOARD_SDA, BOARD_KEYBOARD_SCL);
        Wire.beginTransmission(addr);
        Wire.endTransmission(true);
    }

    if (!keypad.begin(addr, &Wire)) {
        // Serial.println("keypad not found, check wiring & pullups!");
        log_e("keypad not found, check wiring & pullups!");
        return;
    }

    // configure the size of the keypad matrix.
    // all other pins will be inputs
    keypad.matrix(KEYPAD_ROWS, KEYPAD_COLS);

    // flush the internal buffer
    keypad.flush();
}


static void lvgl_keyboard_read(lv_indev_drv_t * indev, lv_indev_data_t * data){
  char c = -1;
    lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
    bool processed = false;
    int row, col;
    uint32_t k = keypad.getEvent();
    uint8_t v = keypad.available();

    if(k >=KEYPAD_RELEASE_VAL_MIN && k <= KEYPAD_RELEASE_VAL_MAX){ // release event
        k = k - KEYPAD_RELEASE_VAL_MIN;
        state = LV_INDEV_STATE_RELEASED;
        processed = true;
    }   

    if(k >=KEYPAD_PRESS_VAL_MIN && k <= KEYPAD_PRESS_VAL_MAX){ // press event
        k = k - KEYPAD_PRESS_VAL_MIN;
        state = LV_INDEV_STATE_PRESSED;
        processed = true;
    }

    if(processed){
        row = k / KEYPAD_COLS;
        col = (KEYPAD_COLS-1) - k % KEYPAD_COLS;
        c = keymap[row][col];
        //Serial.printf("k=%d, v=%d, press:%d, %d, %c\n", k, v, row, col, c);
    }

    data->state = state;
    data->key = c;
    data->continue_reading = v>1; // call again asap if theres more events;
}


// init lvgl, called after initScreen and startup screen draw
/*virtual*/ void TDeckProKeyboard::initLvgl() {
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = lvgl_keyboard_read;
    //auto m_indev = lv_indev_drv_register(&indev_drv);
    _indev =  lv_indev_drv_register(&indev_drv);
    //lv_indev_set_group(m_indev, retOsGlobalPtr->ui()->default_input_group());
    
}

