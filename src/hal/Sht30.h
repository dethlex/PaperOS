#pragma once
#include <stdint.h>

namespace paperos {

// One read of the onboard SHT30. `valid=false` on I2C/CRC error.
struct IndoorReading {
    bool    valid    = false;
    int16_t temp_c   = 0;   // rounded °C, already includes the calibration offset
    uint8_t humidity = 0;   // %
};

class Sht30 {
public:
    void begin();                               // M5.SHT30.Begin()
    IndoorReading read(int8_t temp_offset = 0); // UpdateData + Get*, +offset, round
};

} // namespace paperos
