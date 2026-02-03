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

static const char* getStatusSymbol(Retcon::LXMF::Message::STATUS status) {
    switch(status) {
        case Retcon::LXMF::Message::STATUS::QUEUEING:
        case Retcon::LXMF::Message::STATUS::SENDING:
            return LV_SYMBOL_REFRESH;
        case Retcon::LXMF::Message::STATUS::RETRY:
            return LV_SYMBOL_LOOP;
        case Retcon::LXMF::Message::STATUS::SENT:
            return LV_SYMBOL_OK;
        case Retcon::LXMF::Message::STATUS::FAILED:
            return LV_SYMBOL_CLOSE;
        case Retcon::LXMF::Message::STATUS::UNKNOWN_DEST:
            return LV_SYMBOL_WARNING;
        default:
            return "";
    }
}

lv_obj_t* UChat::renderMessageInConversation(lv_obj_t* parent, const Retcon::LXMF::Message& message) {
    // Each message gets a full-width row; alignment pushes bubble left or right
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* bubble = lv_label_create(row);
    lv_obj_set_size(bubble, lv_pct(70), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(bubble, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bubble, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(bubble, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);

    string label_text = (message.title.empty() ? "" : (message.title + "\n"))
             + message.content + "\n" + formatRelativeTime(message.timestamp);

    if(message.msgSentByThem(current_conv->info.their_hash)) {
        lv_obj_align(bubble, LV_ALIGN_TOP_LEFT, 0, 0);
    } else {
        const char* sym = getStatusSymbol(message.status);
        if(sym[0] != '\0') {
            label_text += " " + string(sym);
        }
        lv_obj_align(bubble, LV_ALIGN_TOP_RIGHT, 0, 0);
    }

    lv_label_set_text(bubble, label_text.c_str());
    return row;
}

void UChat::sendToCurrentConversation(const char* title, const char* content) {
    if(current_conv == nullptr || current_conv->info.their_hash.size() == 0) {
        Serial.println("[UChat] ERROR: Cannot send - no valid conversation open");
        return;
    }
    Serial.print("[UChat] Sending message to: ");
    Serial.println(current_conv->info.their_hash.toHex().c_str());
    Serial.print("[UChat] Content: '");
    Serial.print(content);
    Serial.println("'");
    auto msg = _rns_service->sendLxmfMsg(current_conv->info.their_hash, title, content);
    Serial.print("[UChat] Message status after sendLxmfMsg: ");
    Serial.println((int)msg->status);
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
        message_container = nullptr;
    }

    lv_obj_set_size(conversation_modal, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(conversation_modal, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(conversation_modal, LV_FLEX_FLOW_COLUMN);

    // Title bar
    lv_obj_t* title = lv_label_create(conversation_modal);
    lv_obj_set_style_border_width(title, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(title, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(title, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
    string title_txt = current_conv->info.their_name;
    if(title_txt.size() < 1) title_txt = current_conv->info.their_hash.toHex();
    lv_label_set_text(title, title_txt.c_str());
    lv_obj_set_size(title, lv_pct(100), LV_SIZE_CONTENT);

    // Scrollable message container
    message_container = lv_obj_create(conversation_modal);
    lv_obj_set_size(message_container, lv_pct(100), 0);
    lv_obj_set_flex_grow(message_container, 1);
    lv_obj_set_flex_flow(message_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(message_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(message_container, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(message_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(message_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(message_container, LV_DIR_VER);

    // Render existing messages
    for (const auto& msg : current_conv->getMessages()) {
        renderMessageInConversation(message_container, msg);
    }

    // Input row
    lv_obj_t* input_row = lv_obj_create(conversation_modal);
    lv_obj_set_size(input_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(input_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(input_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(input_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ta = lv_textarea_create(input_row);
    lv_group_add_obj(_retos->ui()->default_input_group(), ta);
    lv_obj_set_flex_grow(ta, 1);
    lv_obj_set_height(ta, 30);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_border_width(ta, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ta, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ta, 3, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * send_btn = lv_btn_create(input_row);
    lv_obj_set_size(send_btn, 35, 30);
    lv_obj_set_style_border_width(send_btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(send_btn, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(send_btn, send_msg_cb, LV_EVENT_CLICKED, ta);

    lv_obj_t* send_btn_label = lv_label_create(send_btn);
    lv_obj_set_size(send_btn_label, lv_pct(100), LV_SIZE_CONTENT);
    lv_label_set_text(send_btn_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(send_btn_label, _retos->ui()->fg_color(),  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(send_btn_label);

    // Auto-focus the textarea so the user can type immediately
    lv_group_focus_obj(ta);

    // Force full layout calculation (conversation_modal must be resolved first
    // so message_container gets its actual height from flex_grow), then scroll
    lv_obj_update_layout(conversation_modal);
    lv_obj_scroll_to_y(message_container, lv_obj_get_scroll_bottom(message_container), LV_ANIM_OFF);
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
                // Redraw if message is for the current conversation
                // (src matches for received msgs, dest matches for sent msgs)
                shared_ptr<Retcon::LXMF::Message> msg_ptr =
                    std::static_pointer_cast<Retcon::LXMF::Message>(event.data);
                if(msg_ptr && (msg_ptr->src == current_conv->info.their_hash ||
                               msg_ptr->dest == current_conv->info.their_hash)) {
                    drawCurrentConversation(true);
                }
            } else {
                // No conversation open — refresh main menu to show new/updated conversations
                renderMainMenu();
            }
            return HANDLED_PROPOGATE;
        }

        case MESSAGE_UPDATE:
        {
            if(conversation_modal != nullptr && current_conv != nullptr) {
                drawCurrentConversation(true);
            }
            return HANDLED_PROPOGATE;
        }

        default:
            return IGNORED;
    }

}

