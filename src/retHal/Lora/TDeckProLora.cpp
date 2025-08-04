#include "TDeckProLora.h"
#include <RadioLib.h>

static Module radioModule(BOARD_LORA_CS, BOARD_LORA_INT, BOARD_LORA_RST, BOARD_LORA_BUSY);
static volatile bool _transmitting = false; // true = we;re transmitting, false we're listening
static volatile bool _jobs_done = false;
unsigned long _tstart = 0;
unsigned long _tend = 0;


void IRAM_ATTR josbDone() {
    _jobs_done = true;
}


TDeckProLora::TDeckProLora() : radio(&radioModule)  {

}

void TDeckProLora::initLora() {

}
bool TDeckProLora::startLora(LoraConfig config) {
    Serial.println("Starting Lora retHAL");
    
    int state = radio.begin(config.frequency, config.bandwidth, config.sf, config.cr, 0x12, config.power, config.preamble_len);
    if (state == RADIOLIB_ERR_NONE) {
        if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) return true;
        if (radio.setTCXO(2.4) == RADIOLIB_ERR_INVALID_TCXO_VOLTAGE) return true;
        if (radio.setCRC(config.crc) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) return true;
        if (radio.setDio2AsRfSwitch() != RADIOLIB_ERR_NONE) return true;


        if(config.explicitHeader) {
            if(radio.explicitHeader()) return true;
        }
    } else {
        // error
        return true;
    }

    radio.setDio1Action(josbDone);
    radio.startReceive();
    _transmitting = false;
    Serial.println("Lora retHAL started!");
    return false; //it's allll good if we got here
}
bool TDeckProLora::hasPacket() {
    return packetLength() > 0;
}
size_t TDeckProLora::packetLength() {
    if(_jobs_done && _transmitting) {
        radio.finishTransmit();
        _jobs_done = false;
        _transmitting = false;
        //delay(1); // git chip time to settle
        radio.startReceive();
        _tend = millis();
        Serial.print("Lora It took this long: ");
        Serial.println(_tend-_tstart);
        return 0;
    }
    if(!_jobs_done) return 0; // nothing to recv
    return radio.getPacketLength();
}
// true = error
bool TDeckProLora::read(uint8_t* data, uint32_t len) {
    if(!_jobs_done)  {
        Serial.println("ERROR! Jobs not done? why are we reading?");
        return true;
    }
     _jobs_done = false;
     int16_t status = radio.readData(data, len); 
     if(status != RADIOLIB_ERR_NONE) Serial.printf("\n\nRECV ERROR: %d\n\n", status);
     return status != RADIOLIB_ERR_NONE;
}

bool TDeckProLora::transmit(uint8_t* data, uint32_t len) {
    // TODO pause or queue if this happens?
    unsigned int ctr = 1000;
    //wait for up to 1 second if we're still transmitting
    while(_transmitting && ctr-- > 0) {
        if(_jobs_done) {
            // ok clean up and then we can send the next one after a short delay
            radio.finishTransmit();
            _jobs_done = false;
            _transmitting = false;
        }
        delay(1);
    }
    // timed out
    if(_transmitting) {
        Serial.println(".....TRANSMIT IMTEOUT!!!.....");
        return true; 
    } 

    _tstart = millis();
    Serial.println(".....Transmitting.....");
    bool result = radio.startTransmit(data, len) != RADIOLIB_ERR_NONE;
    if(!result) _transmitting = true;
    return result;

    // unsigned long then = millis();
    // bool result = radio.transmit(data, len) != RADIOLIB_ERR_NONE;
    // radio.startReceive();
    // unsigned long now = millis();
    // Serial.print("Lora It took this long: ");
    // Serial.println(now-then);
    // return result;
}
float TDeckProLora::getRSSI() {
    return radio.getRSSI();
}