#pragma once
#include <ArduinoJson.h>
#include <memory>

enum EventType {
    // generic OS event
    OS_EVENT = 0,
    // SLEEP events below are currently UNUSED but saved for posterity
    UNUSED_LIGHT_SLEEP, // about to go down for light sleep
    UNUSED_HIBERNATE, // about to hibernate (deep sleep same as shutdown)

    RAW_SENSOR_EVENT_START, // raw sensor events that are NOT fit for humans but may be used by services to inform processed event
    RAW_GPIO, // raw sensor reading. raw GPIO, raw time data from GPS, raw location data, etc etc
    RAW_TEMP,
    RAW_GPS_LOCATION,
    RAW_GPS_TIME,

    
    PROCESSED_EVENT_START, // start of events that are processed and then fit for apps/humans
    TIME_CHANGE,  // Time change like new NTP or timezone. NOT going to tick every second/minute
    LOCATION_CHANGE, // Significant Location change like GPS. Poll instead for more precise data
    NEW_MESSAGE, // New external message like over LXMF or something

    INTER_SERVICE_COMMS_START, // events that let services communicate with eachother
    SERVICE_REQUEST, // request some data arg[0] is request_ID, other args unused
    SERVICE_RESPONSE, // response with some data arg[0] is request_id this is a response to, arg[1] is 0=success, or error code


    CUSTOM_EVENT_START = 0x80,
    
};


enum EventStatus {
    IGNORED = 0, 
    HANDLED, // will NOT propogate further
    HANDLED_PROPOGATE, // we handled it sure, but still propgate it in case others care

};

#define RETOS_EVENT_NUM_ARGS 4

class BaseService;
struct Event {
    BaseService* src;
    EventType type;

    // 4 ints to hold basic event arguments
    uint32_t args[RETOS_EVENT_NUM_ARGS];
    // ref counted void* if 4 ints doesnt meet your needs
    std::shared_ptr<void> data;
 };