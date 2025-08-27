#pragma once
#include "BaseApp.h"
#include "../services/RnsUtils/LXMFData.h"

class RnsService;

class UChat : public BaseApp {
    public:
    // inherit default ctor
        using BaseApp::BaseApp;

        virtual void start(RetOS* retos) override;
        virtual void tick(const time_t tickMillis) override;
        virtual void stop() override;
        virtual const function<void()> customBackButtonAction() override;
        virtual EventStatus onEvent(const Event& event) override;

        void openConversation(const RNS::Bytes& their_hash);
        void sendToCurrentConversation(const char* title, const char* content);


    protected:
            RnsService * _rns_service;

            void renderMainMenu();
            void renderMessageInConversation(const Retcon::LXMF::Message& message);
            void drawCurrentConversation(bool clear=true);

            lv_obj_t * tabview;
            lv_obj_t * msgview;
            lv_obj_t * announceview;
            lv_obj_t * statusview;


            lv_obj_t * conversation_modal = nullptr;
            Retcon::LXMF::Conversation *current_conv = nullptr;

            set<shared_ptr<Retcon::LXMF::Message>> _queued_msgs;


};