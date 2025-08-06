#pragma once
#include "BaseLora.h"
#include "boards/TDeckPro.h"
#include <RadioLib.h>

class TDeckProLora  : public BaseLora {

    public: 
        const static size_t MAX_LORA_PACKET_SIZE = 256;
        LoraConfig config;
        
        TDeckProLora();
        virtual void initLora();
        virtual bool startLora(LoraConfig config);
        virtual bool hasPacket();
        // true = error
        virtual size_t read(uint8_t* data, uint32_t len);
        virtual bool transmit(uint8_t* data, uint32_t len);
        virtual float getRSSI();

    protected:
        SX1262 radio;

    friend void loraTask(void* in);
};