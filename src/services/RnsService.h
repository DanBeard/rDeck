#pragma once
#include "BaseService.h"

#include <queue>
#include <map>
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
#include "RnsUtils/LXMFData.h"



class RnsService: public BaseService {
    
    friend class LoraInterface;


public:

    static const size_t max_number_queued_msgs = 5;
    static const size_t max_number_retries = 4;
    static const time_t packet_timeout_secs = 10;

    explicit RnsService(uint8_t id);
    virtual void start(RetOS* retos) override; // called after construction once the OS is ready to launch services
    virtual void tick(const unsigned long tmillis) override; // called periodically by OS so you can do work. Time varies by sleep and power level but ~1-3ms while awake
    void announce();

    bool isValid = false;

    JsonDocument userInfo;
    void saveUserInfo();

    RNS::Destination lxmf_delivery_src;
    RNS::Reticulum reticulum;

    // send an LXmfMesssage. Pass in an updater function that wi;; be called when status changes with the message object
    typedef function<void(const shared_ptr<Retcon::LXMF::Message>&)> msg_update_cb;

    shared_ptr<Retcon::LXMF::Message>& sendLxmfMsg(const RNS::Bytes dest, const string &title, const string &contents);
    const queue<shared_ptr<Retcon::LXMF::Message>>& queuedMsgs() const;

    static constexpr const char* settingsSection = "reticulum";
    static bool drawSettings(lv_obj_t * column, Settings* settings);
    static void applySettings();
    static void mergeLoraSettings(LoraConfig& config);
    //functions to get info

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

    boolean _sending_message = false;
    shared_ptr<Retcon::LXMF::Message> _current_sending_msg;
    RNS::Packet *_sending_packet;
    std::queue<shared_ptr<Retcon::LXMF::Message>> _send_msg_queue;

    uint8_t _num_retries = 0;
    // actually do the tranmit
    void transmitMsg(shared_ptr<Retcon::LXMF::Message> &msg);
    
    void sendMessageUpdateEvent(shared_ptr<Retcon::LXMF::Message> &msg);

    friend void transmit_delivery_cb(const RNS::PacketReceipt &receipt);
    friend void transmit_timeout_cb(const RNS::PacketReceipt &receipt);

};