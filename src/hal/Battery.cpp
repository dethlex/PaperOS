#include "Battery.h"
#include "util/BatteryCurve.h"
#include <M5EPD.h>

namespace paperos {

void Battery::begin() {
    M5.BatteryADCBegin();
}

BatteryReading Battery::read() {
    BatteryReading r;
    r.millivolts = static_cast<uint16_t>(M5.getBatteryVoltage());
    r.percent    = batteryPercentFromMv(r.millivolts);
    return r;
}

} // namespace paperos
