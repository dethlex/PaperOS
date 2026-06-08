#pragma once
#include <stdint.h>

namespace paperos {

// Estimate charge (%) of a single Li-Po cell from its resting voltage (mV).
// Piecewise-linear interpolation over a hardcoded table; result clamped to 0..100.
uint8_t batteryPercentFromMv(uint16_t mv);

} // namespace paperos
