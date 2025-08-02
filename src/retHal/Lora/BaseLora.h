#pragma once
#include <stdint.h>
#include <string.h>

struct LoraConfig {
    float frequency; // eg 914.875 
    float bandwidth; // eg 125, 250, etc
    uint8_t sf; // spreading factor
    uint8_t cr; // coading rate
    int8_t power; // eg. 20dbm
    uint16_t preamble_len; //18
    uint8_t crc; // 0= disable, 1 or 2 
    bool explicitHeader;
};

class BaseLora
{
public:

    virtual void initLora() = 0;
    virtual bool startLora(LoraConfig config) = 0;
    virtual bool hasPacket() = 0;
    virtual size_t packetLength() = 0;
    // true = error
    virtual bool read(uint8_t* data, uint32_t len) = 0;
    virtual bool transmit(uint8_t* data, uint32_t len) = 0;
    virtual float getRSSI() = 0;

};

