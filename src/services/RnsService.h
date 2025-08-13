#pragma once
#include "BaseService.h"


#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Transport.h"
#include "Interface.h"
#include "Log.h"
#include "Bytes.h"
#include "Type.h"
#include "Utilities/OS.h"
#include "RnsUtils/FileSystem.h"
#include "RnsUtils/LoraInterface.h"
#include "RnsUtils/RDeckAnnounceHandler.h"

class RnsService: public BaseService {
    
    friend class LoraInterface;


public:
    explicit RnsService(uint8_t id);
    virtual void start(RetOS* retos); // called after construction once the OS is ready to launch services
    virtual void tick(); // called periodically by OS so you can do work. Time varies by sleep and power level.
    virtual EventStatus onEvent(Event& event);
    void announce();

    bool isValid = false;

    JsonDocument userInfo;
    void saveUserInfo();

    RNS::Destination lxmf_delivery_src;
    RNS::Reticulum reticulum;
    

protected:
    void updateIcon(bool status);
    // just Lora for now, but we could do TCP/UDP/etc in the future over wifi
    BaseLora* _lora;
    FS* _fs;

    RNS::Identity identity;

    // our destination for sending/recing lxmf messages
    
    //RNS::Interfaces::UDPInterface udp_interface("udp");
    RNS::Interfaces::LoRaInterface *lora_interface_impl;
    RNS::Interface lora_interface;

    RNS::FileSystem rns_fs;

    std::shared_ptr<RDeckAnnounceHandler> _announce_handler;


};