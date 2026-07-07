#pragma once
#include "services/CalendarData.h"
#include <stddef.h>
#include <stdint.h>

namespace paperos {

// Compact little-endian blob for NVS. Layout (version 1):
//   [0] version=1  [1] count  [2..5] fetched_unix
//   then count × record (89 B): start(4) end(4) flags(1: bit0=all_day)
//                               title[48] location[32]
// Returns bytes written, or 0 if `cap` is too small.
size_t serializeCalendar(const CalendarData& d, uint8_t* buf, size_t cap);

// Returns false (and sets out.valid=false) if the blob is malformed/empty.
bool deserializeCalendar(const uint8_t* buf, size_t len, CalendarData& out);

} // namespace paperos
