#pragma once
#include <M5EPD.h>
#include "framework/ui/Geometry.h"

namespace paperos {
namespace gamesui {

// Bordered button with a word label. Left-aligned at a fixed inset — never
// textWidth-centered: M5EPD smooth-font textWidth underestimates capital-Cyrillic
// advance (CLAUDE.md §TTF), so centering drifts. `active` draws a double border
// to mark a toggled-on mode (e.g. Flag).
void button(M5EPD_Canvas& c, Rect r, const char* label, bool active);

// Center a SHORT ASCII string (digits) in a rect. textWidth is reliable for
// ASCII, so true-centering is safe here.
void centerAscii(M5EPD_Canvas& c, Rect r, const char* s, int px);

} // namespace gamesui
} // namespace paperos
