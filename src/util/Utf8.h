#pragma once
#include <stdint.h>
#include <stddef.h>

namespace paperos {

// Reads next codepoint from `data` at `*pos`, advances `*pos`.
// Returns U+FFFD if invalid. Returns 0 if past end.
uint32_t utf8Next(const uint8_t* data, size_t len, size_t* pos);

// Convert a CP1251 byte to Unicode codepoint.
uint32_t cp1251ToCodepoint(uint8_t b);

} // namespace paperos
