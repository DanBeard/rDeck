#pragma once
#include "BaseLora.h"
#include "boards/TDeckPro.h"
#include <RadioLib.h>

class TDeckProLora  : public BaseLora {

    public: 
        TDeckProLora();
        virtual void initLora();
        virtual bool startLora(LoraConfig config);
        virtual bool hasPacket();
        virtual size_t packetLength();
        // true = error
        virtual bool read(uint8_t* data, uint32_t len);
        virtual bool transmit(uint8_t* data, uint32_t len);
        virtual float getRSSI();

    protected:
        SX1262 radio;


};