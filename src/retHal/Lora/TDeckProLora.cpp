#include "TDeckProLora.h"
#include <RadioLib.h>

static Module radioModule(BOARD_LORA_CS, BOARD_LORA_INT, BOARD_LORA_RST, BOARD_LORA_BUSY);

TDeckProLora::TDeckProLora() : radio(&radioModule)  {

}

void TDeckProLora::initLora() {

}
bool TDeckProLora::startLora(LoraConfig config) {
    Serial.printf("Starting Lora retHAL");
    
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

    return false; //it's allll good if we got here
}
bool TDeckProLora::hasPacket() {
    return packetLength() > 0;
}
size_t TDeckProLora::packetLength() {
    return radio.getPacketLength();
}
// true = error
bool TDeckProLora::read(uint8_t* data, uint32_t len) {
     return radio.readData(data, len) != RADIOLIB_ERR_NONE;
}

bool TDeckProLora::transmit(uint8_t* data, uint32_t len) {
    return radio.transmit(data, len) != RADIOLIB_ERR_NONE;
}
float TDeckProLora::getRSSI() {
    return radio.getRSSI();
}