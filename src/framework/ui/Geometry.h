#pragma once
#include <stdint.h>

namespace paperos {

struct Point { int16_t x; int16_t y; };

struct Rect {
    int16_t x; int16_t y; int16_t w; int16_t h;
    bool contains(Point p) const {
        return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
    }
};

constexpr int16_t kScreenW = 540;
constexpr int16_t kScreenH = 960;

// Shared header convention so every app's title looks identical and never
// overlaps the content below it. Draw the title with FontFace::Serif at
// kHeaderFontPx, positioned at (kHeaderTextX, kHeaderTextY). The glyph band
// ends around y≈kHeaderTextY+kHeaderFontPx (~54); keep the first content
// element at kContentTopY or lower.
constexpr int    kHeaderFontPx = 36;
constexpr int16_t kHeaderTextX = 30;
constexpr int16_t kHeaderTextY = 18;
constexpr int16_t kContentTopY = 80;   // safe top for the first control/row

} // namespace paperos
