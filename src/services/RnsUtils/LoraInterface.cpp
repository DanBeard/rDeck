#include "LoraInterface.h"
#include <memory>
#include <Log.h>

using namespace RNS;
using namespace RNS::Interfaces;

// transmit 
//static int transmissionState = RADIOLIB_ERR_NONE;
static volatile bool transmittedFlag = false;

static void set_transmit_flag(void){
    transmittedFlag = true;
}

// receive
static volatile bool receivedFlag = false;

static void set_receive_flag(void){
    receivedFlag = true;
}


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
        .power = 22,
        .preamble_len = 18,
        .crc = 2,
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

/// !!! ON READ
///!!!!!! TODO YOU NEED TO STRIP THE HEADER BYTE AND DEAL WITH SPLIT PACKETS!!!
void LoRaInterface::tick(RNS::Interface& interface) {

	if (_online) {
		// Check for incoming packet
        size_t packet_len = _lora->packetLength();
        if(packet_len == 0) return;

        buffer.clear();
        buffer.reserve(packet_len);

        // ugly to modify the vector buffer directly here but it's dang efficient!
        bool error = _lora->read((uint8_t*) buffer.data(), packet_len);
        if(!error){
            Serial.println(F("[SX1262] Received packet!"));

            Serial.print(F("[SX1262] Data:\t\t"));
            //Serial.println(lora_recv_data.c_str());
            // Serial.println("%d", (int)lora_recv_data.toInt());

            Serial.print(F("[SX1262] RSSI:\t\t"));
            Serial.print(_lora->getRSSI());
            //float lora_recv_rssi = radio.getRSSI();
            Serial.println(F(" dBm"));

            interface.handle_incoming(buffer);
        }else{
            Serial.print(F("failed, code "));
        }

	}
}

/// !!! ON WRITE
///!!!!!! TODO YOU NEED TO DEAL WITH TOO BIG OF PACKETS AND SPLIT/ SEND 2 OUT IF BIGGER THAN LORA PACKET SIZE!!!!!

/*virtual*/ void LoRaInterface::send_outgoing(const Bytes& data) {
	DEBUG(toString() + ".on_outgoing: data: " + data.toHex());
	try {
		if (_online) { 
			TRACE("LoRaInterface: sending " + std::to_string(data.size()) + " bytes...");
			// Send packet
            // TODO: Non blocking interrupt driven would be a MUCH better user expeirence. But more complex
            uint8_t header  = random(256) & 0xF0;
            uint8_t buf[255];
            if(data.size() >= 254) {
                TRACE("PACKET TOO BIG! "+ std::to_string(data.size()) + " ........");
                return;
            }
            buf[0] = header;
            memcpy(buf+1, data.data(), data.size());

            int status = _lora->transmit( buf, data.size() + 1);
            TRACE("LoRaInterface: status " + std::to_string(status) + " ........");
		}
		InterfaceImpl::send_outgoing(data);
	}
	catch (std::exception& e) {
		ERROR("Could not transmit on " + toString() + ". The contained exception was: " + e.what());
	}
}