#pragma once
#include "framework/ui/Geometry.h"
#include <M5EPD.h>
#include <string>
#include <functional>

namespace paperos {

class Display;

enum class KeyboardLayout : uint8_t { RU, EN, Symbols };

class Keyboard {
public:
    // Set the initial value and a callback for when user presses Enter.
    void open(const std::string& initial, std::function<void(const std::string&)> on_done);

    // Render keyboard + value field. The caller draws their own UI in the top half.
    void render(M5EPD_Canvas& canvas);

    // Returns true if the touch was consumed by the keyboard.
    // If user pressed Enter, calls on_done. Caller must close() afterward.
    bool onTouch(int16_t x, int16_t y, Display& display);

    bool isOpen() const { return is_open_; }
    void close() { is_open_ = false; }

    const std::string& value() const { return value_; }
    void setMasked(bool m) { masked_ = m; }

private:
    KeyboardLayout layout_ = KeyboardLayout::EN;
    std::string value_;
    bool is_open_ = false;
    bool masked_ = false;
    // Shift state: single-shot — tap SHF, next letter is uppercase, then auto-revert.
    // Tap SHF again before typing a letter to cancel. Non-letter keys
    // (BSP/ENT/SPACE/SYM/RU/EN/digits/symbols) don't consume the shift.
    bool shift_ = false;
    std::function<void(const std::string&)> on_done_;

    // Bottom-anchored: 4 rows of ~84 px (shorter than the old kScreenH/2 = 120 px
    // rows). The value field sits just above at kKbTop-80.
    static constexpr int16_t kKbTop = 624;
    void drawValueField(M5EPD_Canvas& c);
    void drawKeys(M5EPD_Canvas& c);
    const char* const* currentRows(int& row_count, int& cols_per_row) const;
};

} // namespace paperos
