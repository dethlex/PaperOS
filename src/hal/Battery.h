#pragma once
#include <stdint.h>

namespace paperos {

// A single battery reading. percent is estimated via BatteryCurve.
struct BatteryReading {
    uint16_t millivolts = 0;
    uint8_t  percent     = 0;
};

class Battery {
public:
    void begin();            // calls M5.BatteryADCBegin() just in case (M5.begin already enables the ADC)
    BatteryReading read();   // mv = M5.getBatteryVoltage() (averaged over 8 samples) -> percent
};

} // namespace paperos
