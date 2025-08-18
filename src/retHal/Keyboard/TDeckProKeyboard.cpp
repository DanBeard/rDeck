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

#define SYMBOL_BUTTON 0x1A
#define SHIFT_BUTTON 0x0F
#define MIC_BUTTON 0x0B
#define SPEAKER_BUTTON 0x07

extern uint64_t time_of_last_action;

const char keymap[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', LV_KEY_BACKSPACE}, //backspace
    {'2', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '$', LV_KEY_ENTER},  //enter/LF
    {' ', '?', 'm', ' ', ' ', SHIFT_BUTTON, MIC_BUTTON, ' ', SYMBOL_BUTTON, SHIFT_BUTTON},
};

const char keymap_shift[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P'},
    {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', LV_KEY_BACKSPACE}, //backspace
    {'2', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '$', LV_KEY_ENTER},  //enter/LF
    {' ', '?', 'M', ' ', ' ', SHIFT_BUTTON, MIC_BUTTON, ' ', SYMBOL_BUTTON, SHIFT_BUTTON},
};

const char keymap_symbol[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'#', '1', '2', '3', '(', ')', '_', '-', '+', '@'},
    {'*', '4', '5', '6', '/', ':', ';', '\'', '"', LV_KEY_BACKSPACE}, //backspace
    {'2', '7', '8', '9', '?', '!', '`', '.', SPEAKER_BUTTON, LV_KEY_ENTER},  //enter/LF
    {' ', '?', '0', ' ', ' ', SHIFT_BUTTON, '0', ' ', SYMBOL_BUTTON, SHIFT_BUTTON},
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

static boolean is_shift = false; // was shift pressed before?
static boolean is_symbol = false; // was symbol pressed before?

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
        if(is_shift) {
            c = keymap_shift[row][col];
            is_shift = false;
        } else if(is_symbol) {
            c = keymap_symbol[row][col];
            is_symbol = false;
        } else {
            c = keymap[row][col];
        }
        
        //Serial.printf("k=%d, v=%d, press:%d, %d, %c\n", k, v, row, col, c);
        time_of_last_action = millis();
    }

    // normal key logic
    if(c != SYMBOL_BUTTON && c != SHIFT_BUTTON && c!= MIC_BUTTON && c!= SPEAKER_BUTTON ) {
        data->state = state;
        data->key = c;
        data->continue_reading = v>1; // call again asap if theres more events;
    } else if(processed) {
        // only worry about release
        if(state == LV_INDEV_STATE_RELEASED) {
            if(c==SYMBOL_BUTTON) is_symbol = !is_symbol;
            if(c==SHIFT_BUTTON) is_shift = !is_shift;

            // not sure what to do with these yet O.o
            if(c==MIC_BUTTON) {}
            if(c==SPEAKER_BUTTON) {}
        }
    }

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

