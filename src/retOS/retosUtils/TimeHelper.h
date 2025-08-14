#pragma once
#include <string>
#include <sys/time.h>

class TimeHelper {


public:
    void setTime(time_t epoch_secs);
    void setPosixTimezone(char* timezone_str);
    //void setLocation(double lat, double lon);

    void saveTimeToSd();
    void loadTimeFromSd();

};