#include "UChat.h"
#include "../services/RnsService.h"
#include "../services/RnsUtils/LXMFData.h"

using namespace Retcon::LXMF;

UChat * uchat_ptr;

// Format timestamp as relative time (2s, 5m, 3h, 2d)
static string formatRelativeTime(time_t timestamp) {
    if (timestamp == 0) return "";
    time_t now = time(nullptr);
    time_t diff = now - timestamp;
    if (diff < 0) diff = 0;  // Handle future timestamps
    if (diff < 60) return std::to_string(diff) + "s";
    if (diff < 3600) return std::to_string(diff / 60) + "m";
    if (diff < 86400) return std::to_string(diff / 3600) + "h";
    return std::to_string(diff / 86400) + "d";
}

/*virtual */ void UChat::start(RetOS* retos) {

    _rns_service = retos->fetchService<RnsService>();
    renderMainMenu();

    // for things like lvgl callbacks
    uchat_ptr = this;

}

/*virtual */ void UChat::tick(const unsigned long tickMillis) {
    
}

/*virtual */ void UChat::stop() {
    uchat_ptr = nullptr;
}

void UChat::renderMessageInConversation(const Retcon::LXMF::Message& message) {
    if(message.msgSentByThem(current_conv->info.their_hash)) {
        lv_obj_t* them = lv_label_create(conversation_modal);
        lv_obj_add_flag(them, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_set_size(them, lv_pct(70), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(them, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(them, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(them, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        string label_text = (message.title.empty() ? "" : (message.title + "\n"))
                 + message.content + "\n" + formatRelativeTime(message.timestamp);
        lv_label_set_text(them, label_text.c_str());
    } else {
        lv_obj_t* spacer = lv_obj_create(conversation_modal);
        lv_obj_add_flag(spacer, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_set_size(spacer, lv_pct(29), LV_SIZE_CONTENT);

        lv_obj_t* me = lv_label_create(conversation_modal);
        lv_obj_set_size(me, lv_pct(70), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(me, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(me, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(me, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        string label_text = (message.title.empty() ? "" : (message.title + "\n"))
                 + message.content + "\n" + formatRelativeTime(message.timestamp);
        lv_label_set_text(me, label_text.c_str());    
    }

}

void UChat::sendToCurrentConversation(const char* title, const char* content) {
    if(current_conv == nullptr || current_conv->info.their_hash.size() == 0) {
        Serial.println("[UChat] ERROR: Cannot send - no valid conversation open");
        return;
    }
    auto msg = _rns_service->sendLxmfMsg(current_conv->info.their_hash, title, content);
    _queued_msgs.insert(msg);
}

static void send_msg_cb(lv_event_t * event) {
    lv_obj_t* btm = lv_event_get_target(event);
    lv_obj_t* ta = (lv_obj_t*) lv_event_get_user_data(event);
    const char* msg_txt = lv_textarea_get_text(ta);
    // TODO no titles for now.
    uchat_ptr->sendToCurrentConversation("", msg_txt);
    lv_textarea_set_text(ta, "");
}

void UChat::openConversation(const RNS::Bytes& their_hash) {
    Serial.print("[UChat] openConversation called with hash: ");
    Serial.println(their_hash.toHex().c_str());

    if(conversation_modal != nullptr) {
        lv_obj_del(conversation_modal);
        conversation_modal = nullptr;
    }

    current_conv = Retcon::LXMF::loadAsCurrentConversation(their_hash);

    Serial.print("[UChat] After load, current_conv->info.their_hash: ");
    Serial.println(current_conv->info.their_hash.toHex().c_str());

    conversation_modal = lv_obj_create(screen);
    drawCurrentConversation(false);
}

void UChat::drawCurrentConversation(bool clear) {
    if(clear) {
        lv_obj_clean(conversation_modal);
    }

    lv_obj_set_size(conversation_modal, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(conversation_modal, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(conversation_modal, LV_FLEX_FLOW_ROW);

    lv_obj_t* title = lv_label_create(conversation_modal);
    lv_obj_set_style_border_width(title, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(title, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(title, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
    string title_txt = current_conv->info.their_name;

    if(title_txt.size() < 1) title_txt = current_conv->info.their_hash.toHex();

    lv_label_set_text(title, title_txt.c_str());
    lv_obj_set_size(title, lv_pct(100), LV_SIZE_CONTENT);

    // render existing messages in the conversation
    for (const auto& msg : current_conv->getMessages()) {
        renderMessageInConversation(msg);
    }

    // text input
    lv_obj_t * ta = lv_textarea_create(conversation_modal);
    lv_group_add_obj(_retos->ui()->default_input_group(), ta);
    lv_obj_add_flag(ta, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_set_flex_grow(ta, 3); // GROW!
    lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ta, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * send_btn = lv_btn_create(conversation_modal);
    lv_obj_set_size(send_btn, 50, 50);

    lv_obj_set_style_border_width(send_btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(send_btn, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(send_btn, send_msg_cb, LV_EVENT_CLICKED, ta);

    lv_obj_t* send_btn_label = lv_label_create(send_btn);
    lv_obj_set_size(send_btn_label, lv_pct(100), LV_SIZE_CONTENT);
    lv_label_set_text(send_btn_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(send_btn_label, _retos->ui()->fg_color(),  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(send_btn_label);
}

const function<void()> UChat::customBackButtonAction() {
    if(conversation_modal != nullptr) {
        return [this]() {
            lv_obj_del(conversation_modal);
            conversation_modal = nullptr;
        };
    }

    // default if modal isn't open
    return function<void()>();
}

static vector<RNS::Bytes> announce_list_lookup;
static vector<RNS::Bytes> conversation_list_lookup;

static void announce_click_callback(lv_event_t * event) {
    uint16_t row = (uint32_t) lv_event_get_user_data(event);
    Serial.print("Announce click row: ");
    Serial.print(row);
    Serial.print(" of ");
    Serial.println(announce_list_lookup.size());

    if (row >= announce_list_lookup.size()) {
        Serial.println("ERROR: Announce row out of bounds!");
        return;
    }
    RNS::Bytes hash = announce_list_lookup[row];
    Serial.print("Announce clicked hash: ");
    Serial.println(hash.toHex().c_str());
    if (hash.size() == 0) {
        Serial.println("ERROR: Announce hash is empty!");
        return;
    }
    uchat_ptr->openConversation(hash);
}

static void conversation_click_callback(lv_event_t * event) {
    uint16_t row = (uint32_t) lv_event_get_user_data(event);
    Serial.print("Conversation click row: ");
    Serial.print(row);
    Serial.print(" of ");
    Serial.println(conversation_list_lookup.size());

    if (row >= conversation_list_lookup.size()) {
        Serial.println("ERROR: Conversation row out of bounds!");
        return;
    }
    RNS::Bytes hash = conversation_list_lookup[row];
    Serial.print("Conversation clicked hash: ");
    Serial.println(hash.toHex().c_str());
    if (hash.size() == 0) {
        Serial.println("ERROR: Conversation hash is empty!");
        return;
    }
    uchat_ptr->openConversation(hash);
}

void UChat::renderMainMenu() {
    lv_obj_clean(screen);

    tabview = lv_tabview_create(screen, LV_DIR_BOTTOM, 35);
    lv_obj_set_size(tabview, lv_pct(100) , lv_pct(100));
    lv_obj_set_style_pad_all(tabview, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    msgview = lv_tabview_add_tab(tabview, "Msgs");
    announceview = lv_tabview_add_tab(tabview, "Announce");
    statusview = lv_tabview_add_tab(tabview, "Status");

    lv_obj_set_size(announceview, lv_pct(100) , lv_pct(100));
    lv_obj_set_style_pad_all(announceview, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // lv_obj_set_style_bg_color(msgview, _retos->ui()->bg_color(),  LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(announceview, _retos->ui()->bg_color(),  LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(statusview, _retos->ui()->bg_color(),  LV_STATE_DEFAULT);

    lv_obj_t *tabview_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_border_width(tabview_btns, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(tabview_btns, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(tabview_btns, LV_BORDER_SIDE_TOP, LV_STATE_DEFAULT);


    set<ConversationMetaInfo> *conversations = getAllConversationInfo();
    if(conversations->size() == 0) {
        lv_obj_t *label2 = lv_label_create(msgview);
        lv_obj_set_size(label2, LV_SIZE_CONTENT,LV_SIZE_CONTENT);
        lv_obj_set_align(label2, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label2, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label2, "No Conversations");
    } else {
        conversation_list_lookup.clear();
        conversation_list_lookup.reserve(conversations->size());

        lv_obj_t * list = lv_list_create(msgview);
        lv_obj_set_size(list, lv_pct(100), lv_pct(100));
        lv_obj_set_style_border_width(list, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(list, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);

        uint16_t i = 0;
        for(const ConversationMetaInfo &cmi : *conversations) {
            string display_name = cmi.their_name.empty() ?
                cmi.their_hash.toHex().substr(0,12) + "..." : cmi.their_name;
            string display = display_name + "\n" + formatRelativeTime(cmi.last_message_at);

            conversation_list_lookup.push_back(cmi.their_hash);
            lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_EDIT, display.c_str());
            lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(btn, conversation_click_callback, LV_EVENT_CLICKED, (void*)i++);
        }
    }

    const set<AnnounceData> *announces = getAnnounceData();
    if(announces->size() == 0) {
        lv_obj_t *label2 = lv_label_create(announceview);
        lv_obj_set_size(label2, LV_SIZE_CONTENT,LV_SIZE_CONTENT);   /// 1
        lv_obj_set_align(label2, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label2, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label2, "No Announces");
    } else {
        announce_list_lookup.clear();
        announce_list_lookup.reserve(announces->size());

        lv_obj_t * list = lv_list_create(announceview);
        lv_obj_set_size(list, lv_pct(100), lv_pct(100));
        lv_obj_set_style_border_width(list, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(list, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);

        uint16_t i = 0;
        for(const AnnounceData &ad: *announces) {
            string dn = ad.displayName() + "\n" + formatRelativeTime(ad.last_heard);
            announce_list_lookup.push_back(ad.dest);
            lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_EDIT, dn.c_str());
            lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(btn, announce_click_callback, LV_EVENT_CLICKED, (void*)i++);
        }
    }

    // Status tab - show identity and network info
    lv_obj_set_flex_flow(statusview, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(statusview, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Identity label
    lv_obj_t *id_label = lv_label_create(statusview);
    lv_obj_set_style_text_color(id_label, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    string id_text = "Identity: " + _rns_service->lxmf_delivery_src.hash().toHex().substr(0, 12) + "...";
    lv_label_set_text(id_label, id_text.c_str());

    // LoRa status label
    lv_obj_t *lora_label = lv_label_create(statusview);
    lv_obj_set_style_text_color(lora_label, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    string lora_text = "LoRa: ";
    lora_text += _rns_service->isValid ? "Connected" : "Disconnected";
    lv_label_set_text(lora_label, lora_text.c_str());

    // Queue status label
    lv_obj_t *queue_label = lv_label_create(statusview);
    lv_obj_set_style_text_color(queue_label, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    string queue_text = "Queued msgs: " + std::to_string(_rns_service->queuedMsgs().size());
    lv_label_set_text(queue_label, queue_text.c_str());

}


EventStatus UChat::onEvent(const Event& event) {

    switch(event.type) {
        case NEW_MESSAGE:
        {
            if(conversation_modal != nullptr && current_conv != nullptr) {
                // Only redraw if message is for the current conversation
                shared_ptr<Retcon::LXMF::Message> msg_ptr =
                    std::static_pointer_cast<Retcon::LXMF::Message>(event.data);
                if(msg_ptr && msg_ptr->src == current_conv->info.their_hash) {
                    drawCurrentConversation(true);
                }
            }
            return HANDLED_PROPOGATE;
        }

        case MESSAGE_UPDATE:
        {
            // Redraw if conversation is open (status updates are rare enough this is fine)
            if(conversation_modal != nullptr && current_conv != nullptr) {
                drawCurrentConversation(true);
            }
            return HANDLED_PROPOGATE;
        }

        default:
            return IGNORED;
    }

}

