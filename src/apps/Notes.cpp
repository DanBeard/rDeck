#include "Notes.h"
#include "lvgl.h"


/*virtual */ void NotesApp::start(JsonDocument& args, RetOS* retos) {

    lv_obj_t * ta = lv_textarea_create(screen);
    lv_group_add_obj(retos->ui()->default_input_group(), ta);

    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_width(ta, lv_pct(100));   /// 1
    lv_obj_set_height(ta, lv_pct(100));
    lv_textarea_set_text(ta, "TEXT123");
    //lv_obj_set_height(ta, LV_SIZE_CONTENT);

    //lv_obj_add_state(ta, LV_STATE_FOCUSED); /*To be sure the cursor is visible*/

}

/*virtual */ void NotesApp::loop() {
    
}

/*virtual */ void NotesApp::stop() {
    
}

/*virtual*/ EventStatus NotesApp::onEvent(Event& event){
    return IGNORED;
}
