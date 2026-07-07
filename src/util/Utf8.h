#pragma once
#include <stdint.h>
#include <stddef.h>

namespace paperos {

// Reads next codepoint from `data` at `*pos`, advances `*pos`.
// Returns U+FFFD if invalid. Returns 0 if past end.
uint32_t utf8Next(const uint8_t* data, size_t len, size_t* pos);

// Convert a CP1251 byte to Unicode codepoint.
uint32_t cp1251ToCodepoint(uint8_t b);

// Number of UTF-8 codepoints in the first `len` bytes of `s` (a byte starts
// a codepoint unless its top two bits are "10" — a continuation byte).
size_t utf8CountCp(const char* s, size_t len);

// Byte index after advancing `n_cp` codepoints from byte offset `pos`
// (clamped to `len`).
size_t utf8AdvanceCp(const char* s, size_t len, size_t pos, size_t n_cp);

// Copy at most `max_cp` codepoints of NUL-terminated `src` into `dst`
// (capacity `cap` bytes, always NUL-terminated, codepoints never split).
// If `src` did not fit, appends an ellipsis "…" (3 bytes are reserved for it
// plus 1 for the NUL). cap < 4 yields an empty string. Returns bytes written
// excluding the NUL. Proportional TTF rendering has no reliable textWidth for
// cyrillic (see CLAUDE.md) — codepoint budgets are the project-wide idiom.
size_t utf8Clip(char* dst, size_t cap, const char* src, int max_cp);

} // namespace paperos
