#pragma once
#include "services/WeatherData.h"
#include <stddef.h>
#include <stdint.h>

namespace paperos {

// Compact little-endian blob for NVS. Layout (version 1):
//   [0] version=1  [1] day_count  [2..3] current_c  [4] current_wmo
//   [5..8] fetched_unix  then per day: [tmax i16][tmin i16][wmo u8][weekday u8]
// Returns bytes written, or 0 if `cap` is too small.
size_t serializeWeather(const WeatherData& d, uint8_t* buf, size_t cap);

// Returns false (and sets out.valid=false) if the blob is malformed/empty.
bool deserializeWeather(const uint8_t* buf, size_t len, WeatherData& out);

} // namespace paperos
