#pragma once
#include "BaseLora.h"
#include "boards/TDeckPro.h"
#include <RadioLib.h>

class TDeckProLora  : public BaseLora {

    public: 

        TDeckProLora();
        virtual void initLora();
        virtual bool startLora(const LoraConfig &config) override;
        virtual bool changeConfig(const LoraConfig &config) override;
        virtual bool hasPacket();
        // true = error
        virtual size_t read(uint8_t* data, uint32_t len);
        virtual bool transmit(uint8_t* data, uint32_t len);
        virtual float getRSSI();

    protected:
        SX1262 radio;

    friend void loraTask(void* in);
};