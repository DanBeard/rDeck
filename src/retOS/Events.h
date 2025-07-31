#pragma once
#include <ArduinoJson.h>


enum EventType {
    OS_EVENT = 0,
    KEYBOARD,
    TOUCH,
    TIME,  // like NTP
    LOCATION, // like GPS
    CUSTOM_EVENT_START = 0x80,
    
};

enum EventStatus {
    IGNORED = 0, 
    HANDLED, // will NOT propogate further
    HANDLED_PROPOGATE, // we handled it sure, but still propgate it in case other care

};

struct Event {
    uint8_t src_id;
    EventType type;
    JsonObject args;
 };