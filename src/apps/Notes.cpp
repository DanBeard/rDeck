#include "Notes.h"
#include "lvgl.h"


/*virtual */ void NotesApp::start(RetOS* retos) {

    // don't sleep while this app is open
    _keep_awake = true;

    ta = lv_textarea_create(screen);
    lv_group_add_obj(retos->ui()->default_input_group(), ta);

    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_width(ta, lv_pct(100));   /// 1
    lv_obj_set_height(ta, lv_pct(100));

    FS* fs = retos->hal().fs;
    if(!fs->exists(notesPath)){
        File file = fs->open(notesPath, FILE_WRITE);
        if(!file){
            Serial.println("Failed to open file for writing");
            lv_textarea_set_text(ta, "error opening file for writing");
            return;
        }

        // Start with just a title
        file.write((uint8_t*)"# Notes\n", strnlen("# Notes\n", 16));
        file.close();
    }


   
    //lv_textarea_set_text(ta, "TEXT123");
    //lv_obj_set_height(ta, LV_SIZE_CONTENT);

    //lv_obj_add_state(ta, LV_STATE_FOCUSED); /*To be sure the cursor is visible*/

    // Load notes into the textarea
    File file = fs->open(notesPath, FILE_READ);
    if(!file){
            Serial.println("Failed to open file for reading");
            lv_textarea_set_text(ta, "error opening file for reading");
            return;
        }

    // write contents to textarea
    lv_textarea_set_text(ta, file.readString().c_str());

    file.close();
}

/*virtual */ void NotesApp::tick() {
    
}

/*virtual */ void NotesApp::stop() {
    FS* fs = _retos->hal().fs;
    File file = fs->open(notesPath, FILE_WRITE);
    if(!file){
            Serial.println("Failed to open file for writing in close");
            return;
        }

    // write contents to textarea
    const char* data = lv_textarea_get_text(ta);
    size_t len = strnlen(data,2*1000*1000);
    file.write((uint8_t*) data, len);
    file.close();
}

