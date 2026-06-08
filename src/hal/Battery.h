#pragma once
#include <stdint.h>

namespace paperos {

// Одно чтение батареи. percent — оценка по BatteryCurve.
struct BatteryReading {
    uint16_t millivolts = 0;
    uint8_t  percent     = 0;
};

class Battery {
public:
    void begin();            // на всякий случай M5.BatteryADCBegin() (M5.begin уже включает ADC)
    BatteryReading read();   // mv = M5.getBatteryVoltage() (усреднён 8 семплов) -> percent
};

} // namespace paperos
