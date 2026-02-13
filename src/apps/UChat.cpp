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
        case Retcon::LXMF::Message::STATUS::PROPOGATION_NODE:
            return LV_SYMBOL_UPLOAD;
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

void UChat::triggerAnnounce() {
    if (_rns_service) {
        _rns_service->announce();
        Serial.println("[UChat] Manual announce triggered");
    }
}

// Common send logic used by both button click and Enter key
static void do_send_message(lv_obj_t* ta) {
    const char* msg_txt = lv_textarea_get_text(ta);
    if (msg_txt == nullptr || msg_txt[0] == '\0') return;  // Don't send empty messages
    uchat_ptr->sendToCurrentConversation("", msg_txt);
    lv_textarea_set_text(ta, "");
}

static void send_msg_cb(lv_event_t * event) {
    lv_obj_t* ta = (lv_obj_t*) lv_event_get_user_data(event);
    do_send_message(ta);
}

// Called when Enter is pressed in the textarea (LV_EVENT_READY)
static void ta_enter_cb(lv_event_t * event) {
    lv_obj_t* ta = lv_event_get_target(event);
    do_send_message(ta);
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
    // Send message when Enter is pressed
    lv_obj_add_event_cb(ta, ta_enter_cb, LV_EVENT_READY, nullptr);

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

static void announce_btn_callback(lv_event_t * event) {
    if (uchat_ptr) {
        uchat_ptr->triggerAnnounce();
    }
}

// Pagination callbacks
static void announce_next_cb(lv_event_t * event) {
    if (uchat_ptr) uchat_ptr->nextAnnouncePage();
}
static void announce_prev_cb(lv_event_t * event) {
    if (uchat_ptr) uchat_ptr->prevAnnouncePage();
}
static void conversation_next_cb(lv_event_t * event) {
    if (uchat_ptr) uchat_ptr->nextConversationPage();
}
static void conversation_prev_cb(lv_event_t * event) {
    if (uchat_ptr) uchat_ptr->prevConversationPage();
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

void UChat::renderConversationList() {
    lv_obj_clean(msgview);

    if(_conversations_cache.size() == 0) {
        lv_obj_t *label = lv_label_create(msgview);
        lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_align(label, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label, "No Conversations");
        return;
    }

    conversation_list_lookup.clear();

    // Use flex layout for list + nav buttons
    lv_obj_set_flex_flow(msgview, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *list = lv_list_create(msgview);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(list, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);

    size_t start = _conversation_page * ITEMS_PER_PAGE;
    size_t end = min(start + ITEMS_PER_PAGE, _conversations_cache.size());
    size_t total_pages = (_conversations_cache.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

    for(size_t i = start; i < end; i++) {
        const ConversationMetaInfo &cmi = _conversations_cache[i];
        string display_name = cmi.their_name.empty() ?
            cmi.their_hash.toHex().substr(0,12) + "..." : cmi.their_name;
        string display = display_name + "\n" + formatRelativeTime(cmi.last_message_at);

        conversation_list_lookup.push_back(cmi.their_hash);
        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_EDIT, display.c_str());
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(btn, conversation_click_callback, LV_EVENT_CLICKED, (void*)(i - start));
    }

    // Navigation row (only if multiple pages)
    if(total_pages > 1) {
        lv_obj_t *nav_row = lv_obj_create(msgview);
        lv_obj_set_size(nav_row, lv_pct(100), 30);
        lv_obj_set_flex_flow(nav_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(nav_row, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(nav_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *prev_btn = lv_btn_create(nav_row);
        lv_obj_set_size(prev_btn, 50, 25);
        lv_obj_t *prev_label = lv_label_create(prev_btn);
        lv_label_set_text(prev_label, LV_SYMBOL_LEFT);
        lv_obj_center(prev_label);
        if(_conversation_page > 0) {
            lv_obj_add_event_cb(prev_btn, conversation_prev_cb, LV_EVENT_CLICKED, nullptr);
        } else {
            lv_obj_add_state(prev_btn, LV_STATE_DISABLED);
        }

        lv_obj_t *page_label = lv_label_create(nav_row);
        lv_obj_set_flex_grow(page_label, 1);
        lv_obj_set_style_text_align(page_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        string page_text = std::to_string(_conversation_page + 1) + "/" + std::to_string(total_pages);
        lv_label_set_text(page_label, page_text.c_str());

        lv_obj_t *next_btn = lv_btn_create(nav_row);
        lv_obj_set_size(next_btn, 50, 25);
        lv_obj_t *next_label = lv_label_create(next_btn);
        lv_label_set_text(next_label, LV_SYMBOL_RIGHT);
        lv_obj_center(next_label);
        if(_conversation_page < total_pages - 1) {
            lv_obj_add_event_cb(next_btn, conversation_next_cb, LV_EVENT_CLICKED, nullptr);
        } else {
            lv_obj_add_state(next_btn, LV_STATE_DISABLED);
        }
    }
}

void UChat::renderAnnounceList() {
    lv_obj_clean(announceview);

    if(_announces_cache.size() == 0) {
        lv_obj_t *label = lv_label_create(announceview);
        lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_align(label, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label, "No Announces");
        return;
    }

    announce_list_lookup.clear();

    // Use flex layout for list + nav buttons
    lv_obj_set_flex_flow(announceview, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *list = lv_list_create(announceview);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(list, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);

    size_t start = _announce_page * ITEMS_PER_PAGE;
    size_t end = min(start + ITEMS_PER_PAGE, _announces_cache.size());
    size_t total_pages = (_announces_cache.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

    for(size_t i = start; i < end; i++) {
        const AnnounceData &ad = _announces_cache[i];
        string dn = ad.displayName() + "\n" + formatRelativeTime(ad.last_heard);
        announce_list_lookup.push_back(ad.dest);
        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_EDIT, dn.c_str());
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(btn, announce_click_callback, LV_EVENT_CLICKED, (void*)(i - start));
    }

    // Navigation row (only if multiple pages)
    if(total_pages > 1) {
        lv_obj_t *nav_row = lv_obj_create(announceview);
        lv_obj_set_size(nav_row, lv_pct(100), 30);
        lv_obj_set_flex_flow(nav_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(nav_row, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(nav_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *prev_btn = lv_btn_create(nav_row);
        lv_obj_set_size(prev_btn, 50, 25);
        lv_obj_t *prev_label = lv_label_create(prev_btn);
        lv_label_set_text(prev_label, LV_SYMBOL_LEFT);
        lv_obj_center(prev_label);
        if(_announce_page > 0) {
            lv_obj_add_event_cb(prev_btn, announce_prev_cb, LV_EVENT_CLICKED, nullptr);
        } else {
            lv_obj_add_state(prev_btn, LV_STATE_DISABLED);
        }

        lv_obj_t *page_label = lv_label_create(nav_row);
        lv_obj_set_flex_grow(page_label, 1);
        lv_obj_set_style_text_align(page_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        string page_text = std::to_string(_announce_page + 1) + "/" + std::to_string(total_pages);
        lv_label_set_text(page_label, page_text.c_str());

        lv_obj_t *next_btn = lv_btn_create(nav_row);
        lv_obj_set_size(next_btn, 50, 25);
        lv_obj_t *next_label = lv_label_create(next_btn);
        lv_label_set_text(next_label, LV_SYMBOL_RIGHT);
        lv_obj_center(next_label);
        if(_announce_page < total_pages - 1) {
            lv_obj_add_event_cb(next_btn, announce_next_cb, LV_EVENT_CLICKED, nullptr);
        } else {
            lv_obj_add_state(next_btn, LV_STATE_DISABLED);
        }
    }
}

void UChat::nextAnnouncePage() {
    size_t total_pages = (_announces_cache.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if(_announce_page < total_pages - 1) {
        _announce_page++;
        renderAnnounceList();
    }
}

void UChat::prevAnnouncePage() {
    if(_announce_page > 0) {
        _announce_page--;
        renderAnnounceList();
    }
}

void UChat::nextConversationPage() {
    size_t total_pages = (_conversations_cache.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if(_conversation_page < total_pages - 1) {
        _conversation_page++;
        renderConversationList();
    }
}

void UChat::prevConversationPage() {
    if(_conversation_page > 0) {
        _conversation_page--;
        renderConversationList();
    }
}

void UChat::renderMainMenu() {
    lv_obj_clean(screen);

    tabview = lv_tabview_create(screen, LV_DIR_BOTTOM, 35);
    lv_obj_set_size(tabview, lv_pct(100) , lv_pct(100));
    lv_obj_set_style_pad_all(tabview, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    msgview = lv_tabview_add_tab(tabview, "Msgs");
    announceview = lv_tabview_add_tab(tabview, "Announce");
    statusview = lv_tabview_add_tab(tabview, "Status");

    // Explicit sizing for all tabs to ensure proper layout (fixes pagination visibility)
    lv_obj_set_size(msgview, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(msgview, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_size(announceview, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(announceview, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_size(statusview, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(statusview, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *tabview_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_border_width(tabview_btns, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(tabview_btns, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(tabview_btns, LV_BORDER_SIDE_TOP, LV_STATE_DEFAULT);

    // Cache data and reset pagination
    _conversation_page = 0;
    _announce_page = 0;

    // Copy conversations to vector for pagination
    set<ConversationMetaInfo> *conversations_ptr = getAllConversationInfo();
    _conversations_cache.clear();
    _conversations_cache.reserve(conversations_ptr->size());
    for(const auto &c : *conversations_ptr) {
        _conversations_cache.push_back(c);
    }

    // Copy announces to vector for pagination
    const set<AnnounceData> *announces_ptr = getAnnounceData();
    _announces_cache.clear();
    _announces_cache.reserve(announces_ptr->size());
    for(const auto &a : *announces_ptr) {
        _announces_cache.push_back(a);
    }

    // Render the lists
    renderConversationList();
    renderAnnounceList();

    // Status tab - show identity and network info
    lv_obj_set_flex_flow(statusview, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(statusview, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Identity label
    lv_obj_t *id_label = lv_label_create(statusview);
    lv_obj_set_style_text_color(id_label, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    string id_text = "Identity: ";
    if (_rns_service->lxmf_delivery_src) {
        id_text += _rns_service->lxmf_delivery_src.hash().toHex().substr(0, 12) + "...";
    } else {
        id_text += "Initializing...";
    }
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

    // Spacer before announce button
    lv_obj_t *spacer = lv_obj_create(statusview);
    lv_obj_set_size(spacer, lv_pct(100), 10);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(spacer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Announce button
    lv_obj_t *announce_btn = lv_btn_create(statusview);
    lv_obj_set_size(announce_btn, lv_pct(80), 35);
    lv_obj_set_style_border_width(announce_btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(announce_btn, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(announce_btn, announce_btn_callback, LV_EVENT_CLICKED, nullptr);
    lv_group_add_obj(_retos->ui()->default_input_group(), announce_btn);

    lv_obj_t *announce_btn_label = lv_label_create(announce_btn);
    lv_label_set_text(announce_btn_label, LV_SYMBOL_WIFI " Announce");
    lv_obj_set_style_text_color(announce_btn_label, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(announce_btn_label);

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

