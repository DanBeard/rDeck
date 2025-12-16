#include "HelloWorld.h"



/*virtual */ void HelloWorld::start(RetOS* retos) {
    lv_obj_t *label2 = lv_label_create(screen);
    lv_obj_set_width(label2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(label2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(label2, 0);
    lv_obj_set_y(label2, 0);
    lv_obj_set_align(label2, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(label2, retos->ui()->fg_color(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label2, "Hello World!");

}

/*virtual */ void HelloWorld::tick(const unsigned long tickMillis) {
    
}

/*virtual */ void HelloWorld::stop() {
    
}
