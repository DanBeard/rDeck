#include "UChat.h"
#include "../services/RnsService.h"
#include "../services/RnsUtils/LXMFData.h"

using namespace Retcon::LXMF;

/*virtual */ void UChat::start(RetOS* retos) {

    _rns_service = retos->fetchService<RnsService>();
    renderMainMenu();

}

/*virtual */ void UChat::tick(const time_t tickMillis) {
    
}

/*virtual */ void UChat::stop() {
    
}

void UChat::renderMainMenu() {
    lv_obj_clean(screen);

    tabview = lv_tabview_create(screen, LV_DIR_BOTTOM, 35);
    msgview = lv_tabview_add_tab(tabview, "Msgs");
    announceview = lv_tabview_add_tab(tabview, "Announce");
    statusview = lv_tabview_add_tab(tabview, "Status");

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

    set<AnnounceData> *announces = getAnnounceData();
    if(conversations->size() == 0) {
        lv_obj_t *label2 = lv_label_create(announceview);
        lv_obj_set_size(label2, LV_SIZE_CONTENT,LV_SIZE_CONTENT);   /// 1
        lv_obj_set_align(label2, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label2, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label2, "No Announces");
    } else {
        // TODO Actually list announces with buttons that will open a new (or current) conversation view over the main menu
    }

    // TODO status view
    lv_obj_t *label2 = lv_label_create(statusview);
        lv_obj_set_size(label2, LV_SIZE_CONTENT,LV_SIZE_CONTENT);   /// 1
        lv_obj_set_align(label2, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label2, _retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label2, "Status view not implemented yet");

}