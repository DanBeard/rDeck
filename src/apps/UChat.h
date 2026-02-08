#pragma once
#include "BaseApp.h"
#include "../services/RnsUtils/LXMFData.h"

class RnsService;

class UChat : public BaseApp {
    public:
    // inherit default ctor
        using BaseApp::BaseApp;

        virtual void start(RetOS* retos) override;
        virtual void tick(const unsigned long tickMillis) override;
        virtual void stop() override;
        virtual const function<void()> customBackButtonAction() override;
        virtual EventStatus onEvent(const Event& event) override;

        void openConversation(const RNS::Bytes& their_hash);
        void sendToCurrentConversation(const char* title, const char* content);
        void triggerAnnounce();

        // Pagination controls
        void nextAnnouncePage();
        void prevAnnouncePage();
        void nextConversationPage();
        void prevConversationPage();

    protected:
            RnsService * _rns_service;

            void renderMainMenu();
            void renderAnnounceList();
            void renderConversationList();
            lv_obj_t* renderMessageInConversation(lv_obj_t* parent, const Retcon::LXMF::Message& message);
            void drawCurrentConversation(bool clear=true);

            lv_obj_t * tabview;
            lv_obj_t * msgview;
            lv_obj_t * announceview;
            lv_obj_t * statusview;

            lv_obj_t * conversation_modal = nullptr;
            lv_obj_t * message_container = nullptr;
            Retcon::LXMF::Conversation *current_conv = nullptr;

            set<shared_ptr<Retcon::LXMF::Message>> _queued_msgs;

            // Pagination state
            static const size_t ITEMS_PER_PAGE = 15;
            size_t _announce_page = 0;
            size_t _conversation_page = 0;
            vector<Retcon::LXMF::AnnounceData> _announces_cache;
            vector<Retcon::LXMF::ConversationMetaInfo> _conversations_cache;

};