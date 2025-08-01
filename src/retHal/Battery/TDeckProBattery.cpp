#include "TDeckProBattery.h"
#include "boards/TDeckPro.h"
#include <XPowersLib.h>
#include "bq27220.h"

XPowersPPM PPM;
BQ27220 bq27220;

void TDeckProBattery::initBattery() {

    // BQ25896 --- 0x6B
    Wire.beginTransmission(BOARD_I2C_ADDR_BQ25896);
    if (Wire.endTransmission() == 0)
    {
        // battery_25896.begin();
        PPM.init(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, BOARD_I2C_ADDR_BQ25896);
        // Set the minimum operating voltage. Below this voltage, the PPM will protect
        PPM.setSysPowerDownVoltage(3300);

        // Set input current limit, default is 500mA
        PPM.setInputCurrentLimit(3250);

        Serial.printf("getInputCurrentLimit: %d mA\n",PPM.getInputCurrentLimit());

        // Disable current limit pin
        PPM.disableCurrentLimitPin();

        // Set the charging target voltage, Range:3840 ~ 4608mV ,step:16 mV
        PPM.setChargeTargetVoltage(4208);

        // Set the precharge current , Range: 64mA ~ 1024mA ,step:64mA
        PPM.setPrechargeCurr(64);

        // The premise is that Limit Pin is disabled, or it will only follow the maximum charging current set by Limi tPin.
        // Set the charging current , Range:0~5056mA ,step:64mA
        PPM.setChargerConstantCurr(832);

        // Get the set charging current
        PPM.getChargerConstantCurr();

        PPM.enableADCMeasure();

        PPM.enableCharge();

        // Turn off charging function
        // If USB is used as the only power input, it is best to turn off the charging function,
        // otherwise the VSYS power supply will have a sawtooth wave, affecting the discharge output capability.
        // PPM.disableCharge();


        // The OTG function needs to enable OTG, and set the OTG control pin to HIGH
        // After OTG is enabled, if an external power supply is plugged in, OTG will be turned off

        PPM.enableOTG();
        PPM.disableOTG();
        // pinMode(OTG_ENABLE_PIN, OUTPUT);
        // digitalWrite(OTG_ENABLE_PIN, HIGH);

        
    }

    bq27220.init();

}


uint8_t TDeckProBattery::percent() {
    return bq27220.getStateOfCharge();
}

bool TDeckProBattery::charging() {
    return bq27220.getIsCharging();
}