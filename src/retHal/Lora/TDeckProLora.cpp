#include "TDeckProLora.h"
#include <RadioLib.h>
#include "freertos/ringbuf.h"

static Module radioModule(BOARD_LORA_CS, BOARD_LORA_INT, BOARD_LORA_RST, BOARD_LORA_BUSY);
static volatile bool _transmitting = false; // true = we're transmitting, false we're listening
static volatile bool _jobs_done = false;
unsigned long _tstart = 0;
unsigned long _tend = 0;

static TaskHandle_t loraTaskHandle;
static RingbufHandle_t send_buf;
static RingbufHandle_t recv_buf;


void IRAM_ATTR jobsDone() {
    _jobs_done = true;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR( loraTaskHandle, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}


void loraTask(void* in) {
    // map some vars. Almost as good as a this ptr
    TDeckProLora* lora = (TDeckProLora*) in;
    SX1262& radio = lora->radio;
    _transmitting = false;
    _jobs_done= false;


    // init the radio
    Serial.println("Starting Lora retHAL");
    LoraConfig& config = lora->config;

    int state = radio.begin(config.frequency, config.bandwidth, config.sf, config.cr, 0x12, config.power, config.preamble_len);
    if (state == RADIOLIB_ERR_NONE) {
        bool config_error = false;
        if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) config_error = true;
        if (radio.setTCXO(2.4) == RADIOLIB_ERR_INVALID_TCXO_VOLTAGE) config_error = true;
        if (radio.setCRC(config.crc) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) config_error = true;
        if (radio.setDio2AsRfSwitch() != RADIOLIB_ERR_NONE) config_error = true;


        if(config.explicitHeader) {
            if(radio.explicitHeader()) config_error = true;
        }

        if(config_error) {
            // TODO Maybe bring this up to user instead of crashing?
            Serial.println("LORA config error");
            while(1);
        }
    } else {
        // error fatal fixme!!
        while(1) {
            Serial.println("ERROR: Starting Lora retHAL");
            Serial.println(state);
        }
    }
    Serial.println("Finished Lora Rethal");

    radio.setDio1Action(jobsDone);
    radio.startReceive();

    delay(250); // give it a sec to start up
    
    while(true) {

        if(_transmitting && !_jobs_done) {
            //Are we in the middle of transmitting?
            // do nothing
        } else if(_transmitting && _jobs_done) {
            //are we done transmitting?
            radio.finishTransmit();
            _jobs_done = false;
            _transmitting = false;
            // back to recv mode
            radio.startReceive();
            _tend = millis();
            Serial.print("Lora send took this long: ");
            Serial.println(_tend-_tstart);
        } else if(_jobs_done){
            // did we just get a lora packet?
            _jobs_done = false;
            uint8_t data[TDeckProLora::MAX_LORA_PACKET_SIZE];
            size_t packet_len = radio.getPacketLength();
            int16_t status = radio.readData(data, TDeckProLora::MAX_LORA_PACKET_SIZE); 
            if(status != RADIOLIB_ERR_NONE) Serial.printf("\n\nRECV ERROR: %d\n\n", status);
            else {
                 // push to the buffer so the other task can read
                UBaseType_t res =  xRingbufferSend(recv_buf, data, packet_len, pdMS_TO_TICKS(1000));
                if (res != pdTRUE) {
                    Serial.printf("Failed to send item\n");
                }
            }
        }

        // if we're not transmitting and not done with work, look for more work
       if(!_transmitting && !_jobs_done) {   
            UBaseType_t free, read, write, acquire,waiting;
            vRingbufferGetInfo(send_buf, &free, &read,&write, &acquire, &waiting);
            bool any_to_send = waiting > 0;

            // vRingbufferGetInfo(recv_buf, &free, &read,&write, &acquire, &waiting);
            // bool any_to_recv = waiting > 0;

            if(any_to_send) {
                //Receive an item from no-split ring buffer
                size_t item_size;
                uint8_t *data = (uint8_t *)xRingbufferReceive(send_buf, &item_size, pdMS_TO_TICKS(1));
                if(data != NULL) {
                    bool error = radio.startTransmit(data, item_size) != RADIOLIB_ERR_NONE;
                    if(!error) _transmitting = true;
                    vRingbufferReturnItem(send_buf, (void *)data);
                }

            } 
            // else if(radio.getPacketLength() > 0) {
            //     // loop again so we can recv it
            //     // this shouldn't really every happen
            //     _jobs_done = true;
            //     Serial.println("!!!!!! packet length positive but _jobs_done = false");
            // }
            else {
                 // wait until there are some to send or recv
                xTaskNotifyWait(0x00,ULONG_MAX, NULL, pdMS_TO_TICKS(1000));
            }
       }
       
    }
}

TDeckProLora::TDeckProLora() : radio(&radioModule)  {
     // create the ring buffers
    send_buf = xRingbufferCreate(TDeckProLora::MAX_LORA_PACKET_SIZE*4, RINGBUF_TYPE_NOSPLIT);
    recv_buf = xRingbufferCreate(TDeckProLora::MAX_LORA_PACKET_SIZE*4, RINGBUF_TYPE_NOSPLIT);
    if (send_buf == NULL || recv_buf == NULL) {
        printf("Failed to create ring buffer\n");
        // this is fatal. make sure developer sees it and FIXES IT
        while (true)
        {
            Serial.println("Failed to create ring buffer\n");
            delay(250);
        }
    }

}

void TDeckProLora::initLora() {

}
bool TDeckProLora::startLora(LoraConfig new_config) {
    this->config = new_config;
    xTaskCreatePinnedToCore(loraTask, "LoraTask", 10000, this, 15, &loraTaskHandle, 0);
    return false; //it's allll good if we got here
}

bool TDeckProLora::hasPacket() {
    UBaseType_t free, read, write, acquire, waiting;
    vRingbufferGetInfo(recv_buf, &free, &read,&write, &acquire, &waiting);
    //Serial.printf("Lora recv %u packets waiting\n", waiting);
    return waiting > 0;
}

// true = error
size_t TDeckProLora::read(uint8_t* data_out, uint32_t len) {
    size_t item_size;
    uint8_t *data = (uint8_t *)xRingbufferReceive(recv_buf, &item_size, pdMS_TO_TICKS(100));
    if(data != NULL) {
        if(item_size > len) {
             Serial.println("!!! LORA RECV error: Got bigger lora packet than data_out can handle!!");
        } else {
            memcpy(data_out, data, item_size);
        }
        vRingbufferReturnItem(recv_buf, (void *)data);
        return item_size;
    } else {
        Serial.println("!!! LORA RECV error or attempted to read from empty recv buf");
        return 0;
    }
}

bool TDeckProLora::transmit(uint8_t* data, uint32_t len) {
    _tstart = millis();
    Serial.println(".....Transmitting.....");

    // push to the buffer so the other task can read
    UBaseType_t res =  xRingbufferSend(send_buf, data, len, pdMS_TO_TICKS(1000));
    if (res != pdTRUE) {
        Serial.println("!!! ERROR: Lora send buf is full");
    }
    // Alert the other task its time to transmit
    xTaskNotify( loraTaskHandle, 0, eNoAction);
    return false;
}

float TDeckProLora::getRSSI() {
    // TODO: fill from other task and read here
    return 0;// radio.getRSSI();
}