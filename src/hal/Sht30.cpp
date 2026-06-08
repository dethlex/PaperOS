#include "Sht30.h"
#include <M5EPD.h>

namespace paperos {

void Sht30::begin() {
    // M5.begin() already brought up I2C (Wire on 21/22 via touch init);
    // SHT30.Begin() re-calls Wire.begin() but that is idempotent on this bus.
    M5.SHT30.Begin();
}

IndoorReading Sht30::read(int8_t temp_offset) {
    IndoorReading r;
    if (M5.SHT30.UpdateData() != 0) return r;     // valid stays false
    float t = M5.SHT30.GetTemperature();          // °C
    float h = M5.SHT30.GetRelHumidity();          // %
    int ti = static_cast<int>(t >= 0 ? t + 0.5f : t - 0.5f) + temp_offset;
    if (ti >  32767) ti =  32767;
    if (ti < -32768) ti = -32768;
    int hi = static_cast<int>(h + 0.5f);
    if (hi < 0) hi = 0;
    if (hi > 100) hi = 100;
    r.temp_c   = static_cast<int16_t>(ti);
    r.humidity = static_cast<uint8_t>(hi);
    r.valid    = true;
    return r;
}

} // namespace paperos
