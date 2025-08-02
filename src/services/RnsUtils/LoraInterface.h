#pragma once

#include <Interface.h>
#include <Bytes.h>
#include <Type.h>
#include <stdint.h>
#include "retHal/Lora/BaseLora.h"

/// !!! ON READ
///!!!!!! TODO YOU NEED TO STRIP THE HEADER BYTE AND DEAL WITH SPLIT PACKETS!!!

/// !!! ON WRITE
///!!!!!! TODO YOU NEED TO DEAL WITH TOO BIG OF PACKETS AND SPLIT/ SEND 2 OUT IF BIGGER THAN LORA PACKET SIZE!!!!!

namespace RNS { namespace Interfaces {

    class LoRaInterface : public InterfaceImpl {

	public:
		LoRaInterface(BaseLora*);
		//z def get_address_for_if(name):
		//z def get_broadcast_for_if(name):

	public:
		//p def __init__(self, owner, name, device=None, bindip=None, bindport=None, forwardip=None, forwardport=None):
		LoRaInterface(const char* name = "LoRaInterface");
		virtual ~LoRaInterface();

		bool start();
		void stop();
		void tick(RNS::Interface& interface);

		virtual inline std::string toString() const { return "LoRaInterface[" + _name + "]"; }

	protected:
	    //virtual void on_incoming(const Bytes& data);
		virtual void  send_outgoing(const Bytes& data);

		const uint16_t HW_MTU = 508;
		//uint8_t buffer[Type::Reticulum::MTU] = {0};
		const uint8_t message_count = 0;
		Bytes buffer;
        //bool _is_transmitting = false; // For non-blocking transmit

		BaseLora* _lora;

	};

} }


