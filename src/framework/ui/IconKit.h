#pragma once
#include <M5EPD.h>
#include <string>
#include "util/WmoCode.h"   // WeatherCat

namespace paperos {

// One home for every vector glyph (ink=15 on white, canvas primitives only,
// no bitmap assets). Absorbs the former WeatherIcon + BatteryIcon and adds
// launcher app glyphs. Weather/house/drop/battery are 1:1 migrations of the
// old behavior; app() dispatches an app id to its glyph.
class IconKit {
public:
    static void weather(M5EPD_Canvas& c, WeatherCat cat, int x, int y, int s);
    static void house(M5EPD_Canvas& c, int x, int y, int s);
    static void drop (M5EPD_Canvas& c, int x, int y, int s);
    static void battery(M5EPD_Canvas& c, int x, int y, int w, int h, uint8_t percent);
    // Light toggle glyph: filled bulb when on, outline bulb when off.
    static void lightbulb(M5EPD_Canvas& c, int x, int y, int s, bool on);
    // Power-symbol toggle glyph: filled disc when on, outline ring when off/unknown.
    static void power(M5EPD_Canvas& c, int x, int y, int s, bool on);
    // On-screen-keyboard key glyphs (replace the overflowing "BSP"/"SHF" text).
    static void backspace(M5EPD_Canvas& c, int x, int y, int s);  // ⌫ left arrow + X
    static void shift    (M5EPD_Canvas& c, int x, int y, int s);  // ⇧ up arrow
    // Launcher app icon by id: reader→book, ha→house, weather→cloudSun,
    // settings→gear, fileserver→folder, games→dice, else→tile (fallback).
    // Per-game menu glyphs: game_tictactoe→X·O, game_minesweeper→mine,
    // game_fifteen→sliding tiles, game_sudoku→3×3 grid.
    static void app(M5EPD_Canvas& c, const std::string& id, int x, int y, int s);
};

} // namespace paperos
