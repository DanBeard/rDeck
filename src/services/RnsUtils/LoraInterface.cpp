#include "LoraInterface.h"
#include <memory>
#include <Log.h>
#include <algorithm>

using namespace RNS;
using namespace RNS::Interfaces;


LoRaInterface::LoRaInterface(BaseLora* lora ) : _lora(lora), InterfaceImpl("Lora") {

	_IN = true;
	_OUT = true;
	//p self.bitrate = self.r_sf * ( (4.0/self.r_cr) / (math.pow(2,self.r_sf)/(self.r_bandwidth/1000)) ) * 1000
	//_bitrate = ((double)spreading * ( (4.0/coding) / (pow(2, spreading)/(bandwidth/1000.0)) ) * 1000.0);
    _bitrate = ((double)7 * ( (4.0/8) / (pow(2, 7)/(250.0/1000.0)) ) * 1000.0);

}

/*virtual*/ LoRaInterface::~LoRaInterface() {
	stop();
}

bool LoRaInterface::start() {
	_online = (false);
    //Serial.print(("[SX1262] Initializing ... "));
    // TODO load from JSON
    LoraConfig config {
        .frequency =  914.875F,
        .bandwidth =  250.000F,
        .sf = 7,
        .cr = 8,
        .power = 16,
        .preamble_len = 8,
        .crc = 0,
        .explicitHeader = true
    };
    bool error =  _lora->startLora(config);
    if(error) Serial.println("OH NO ERROR IN LORA CONFIG");
    else   {
        Serial.println("LORA CONFIG GOOD!");
        _online = (true);
    }
    return error;
}

void LoRaInterface::stop() {


	_online = (false);
}


void LoRaInterface::tick(RNS::Interface& interface) {

	if (_online) {
        while(_lora->hasPacket()) { 
            Serial.println("Lora Packet Recv!");

            uint8_t lora_packet[MAX_LORA_PACKET_SIZE+1];
            size_t packet_len = _lora->read(lora_packet, MAX_LORA_PACKET_SIZE);
            if(packet_len == 0)  {
                Serial.print(F("ERROR: LoRa READ ERROR "));
                return;
            }
            
            uint8_t header = lora_packet[0];
            uint8_t sequence = header >> 4;
            bool is_split = header & LORA_FLAG_SPLIT;
            //Serial.print(_lora->getRSSI());

            // if we're NOT waiting for another split packet
            // or we are but this one isn't split
            // or we are but one isn't the one we're waiting for
            // then treat it like the start of a new RNS packet
            if(_seq == SEQ_UNSET || !is_split || (_seq != sequence && _seq != SEQ_UNSET)) { 
                buffer.assign(lora_packet + 1, packet_len-1);   
                // if we're not split then it's simple. Just handle the packet;
                if(!is_split) {
                    _seq = SEQ_UNSET; // not waiting for anything any more
                    interface.handle_incoming(buffer);
                    // TODO Clear buffer to free up heap?
                } else {
                    // if we're a split packet then cache it and wait for the next packet
                    _seq = sequence;
                }
            } else {
                // we must be waiting, this is the one we're waiting for and this is the second half
                buffer.append(lora_packet + 1, packet_len-1);
                interface.handle_incoming(buffer);
            }
        }
	}
}


static uint8_t header_id = random(0x0F);
static uint8_t next_header_id() {
    uint8_t next_id = ++header_id % 0x0F;
    return next_id << 4;
};

/*virtual*/ void LoRaInterface::send_outgoing(const Bytes& data) {
	DEBUG(toString() + ".on_outgoing: data: " + data.toHex());
	try {
		if (_online) { 
			TRACE("LoRaInterface: sending " + std::to_string(data.size()) + " bytes...");
			// Send packet
            uint8_t header  = next_header_id(); //random(256) & 0xF0; <--- old code. But this should be faster and more predictable
            if(data.size() > MAX_LORA_PACKET_SIZE - LORA_HEADER_SIZE) {
                header = header | LORA_FLAG_SPLIT;
            }

            uint8_t buf[MAX_LORA_PACKET_SIZE];
            for(uint32_t i=0; i<data.size(); i+=(MAX_LORA_PACKET_SIZE - LORA_HEADER_SIZE)) {
                buf[0] = header;
                size_t size = std::min((size_t)(MAX_LORA_PACKET_SIZE - LORA_HEADER_SIZE), data.size() - i);
                memcpy(buf+1, data.data() + i, size);

                int status = _lora->transmit(buf, size + LORA_HEADER_SIZE);
                TRACE("LoRaInterface: status " + std::to_string(status) + " SENT " + std::to_string(size)+ " bytes (+ 1 header byte)........");
            }
           
		}
		InterfaceImpl::send_outgoing(data);
	}
	catch (std::exception& e) {
		ERROR("Could not transmit on " + toString() + ". The contained exception was: " + e.what());
	}
}