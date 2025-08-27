#include "UChat.h"
#include "../services/RnsService.h"
#include "../services/RnsUtils/LXMFData.h"

using namespace Retcon::LXMF;

UChat * uchat_ptr;

/*virtual */ void UChat::start(RetOS* retos) {

    _rns_service = retos->fetchService<RnsService>();
    renderMainMenu();

    // for things like lvgl callbacks
    uchat_ptr = this;

}

/*virtual */ void UChat::tick(const time_t tickMillis) {
    
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
                 + message.content + "\n" + "time here";
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
                 + message.content + "\n" + "time here";
        lv_label_set_text(me, label_text.c_str());    
    }

}

void UChat::sendToCurrentConversation(const char* title, const char* content) {
    _rns_service->sendLxmfMsg(current_conv->info.their_hash, title, content);
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
    if(conversation_modal != nullptr) {
        lv_obj_del(conversation_modal);
        conversation_modal = nullptr;
    }

    current_conv = Retcon::LXMF::loadAsCurrentConversation(their_hash);

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

static void announce_click_callback(lv_event_t * event) {
    lv_obj_t* list = lv_event_get_target(event);
    uint16_t row, col;
    row = (uint32_t) lv_event_get_user_data(event);
    //lv_table_get_selected_cell(table,&row,&col); 

    Serial.print("----> Clicked: ");
    Serial.print(row);
    // Serial.print(",");
    // Serial.print(col);
    Serial.print("  hash=");
    RNS::Bytes hash = announce_list_lookup[row];

    Serial.println(hash.toHex().c_str());
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
        lv_obj_set_size(label2, LV_SIZE_CONTENT,LV_SIZE_CONTENT);   /// 1
        lv_obj_set_align(label2, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label2, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label2, "No Conversations");
    } else {
        // TODO Actually list conversations with buttons that will open the conversation view over the main menu
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
        // TODO Actually list announces with buttons that will open a new (or current) conversation view over the main menu
        lv_obj_t * list = lv_list_create(announceview);
        // lv_table_set_col_cnt(table, 1);
        // lv_table_set_row_cnt(table, announces->size());
        // lv_table_set_col_width(table,0, lv_pct(100));

        lv_obj_set_size(list, lv_pct(100), lv_pct(100));
        lv_obj_set_style_border_width(list, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(list, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        uint16_t i = 0;  lv_obj_t * btn;
        for(const AnnounceData &ad: *announces) {
            string dn = ad.displayName() + "\nSome time ago.";
            Serial.println(dn.c_str());
            announce_list_lookup.push_back(ad.dest);
            //lv_table_set_cell_value(table, i++, 0, dn.c_str());
            btn = lv_list_add_btn(list, LV_SYMBOL_EDIT, dn.c_str());
            lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(btn, announce_click_callback, LV_EVENT_CLICKED, (void*)i++);
        }

        //lv_obj_add_event_cb(table, announce_click_callback, LV_EVENT_CLICKED, NULL);
    }

    // TODO status view
    lv_obj_t *label2 = lv_label_create(statusview);
        lv_obj_set_size(label2, LV_SIZE_CONTENT,LV_SIZE_CONTENT);   /// 1
        lv_obj_set_align(label2, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label2, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label2, "Status view not implemented yet");

}


EventStatus UChat::onEvent(const Event& event) {

    switch(event.type) {
        case NEW_MESSAGE:
        {
            // TODO: more surgical updates instead of just redrawing the whole dang thing
            // like, is this message even IN the open convo?
            if(conversation_modal != nullptr) {
                drawCurrentConversation(true);
            }
            return HANDLED_PROPOGATE;
        }

        case MESSAGE_UPDATE:
            {
                if(_queued_msgs.size() > 0) {
                    shared_ptr<Retcon::LXMF::Message> msg_ptr = std::static_pointer_cast<Retcon::LXMF::Message>(event.data);
                    // if it's one of our queued messages
                    if(_queued_msgs.find(msg_ptr) != _queued_msgs.end()){
                        // right now just draw everything in a conversation is open.
                        // TODO: more surgical updates instead of just redrawing the whole dang thing
                        if(conversation_modal != nullptr) {
                            drawCurrentConversation(true);
                        }
                        return HANDLED_PROPOGATE;
                    }
                }
               
            }
        
        default:
            return IGNORED;
    }

}

