#pragma once
#include <M5EPD.h>

namespace paperos {

enum class ButtonId : uint8_t { Up = 0, Mid = 1, Down = 2 };  // G39, G38, G37

// Side keys of the M5Paper. We delegate to the M5EPD library's own debounced
// Button objects (M5.BtnL=G37, M5.BtnP=G38, M5.BtnR=G39) instead of reading the
// raw GPIOs ourselves:
//   * M5.begin() already configures these pins (pinMode INPUT — the board has
//     external pull-ups; G34-39 are input-only ESP32 pins with NO internal
//     pull resistors, so our old pinMode(INPUT_PULLUP) was a no-op at best).
//   * M5.update() (called once per loop in main.cpp) polls + debounces them.
// InputRouter still does its own edge/long-press tracking on top of pressed().
class Buttons {
public:
    void begin() {}   // nothing to do — M5.begin() owns pin setup

    bool pressed(ButtonId b) const {
        switch (b) {
            case ButtonId::Up:   return M5.BtnR.isPressed() != 0;  // G39
            case ButtonId::Mid:  return M5.BtnP.isPressed() != 0;  // G38
            case ButtonId::Down: return M5.BtnL.isPressed() != 0;  // G37
        }
        return false;
    }
};

} // namespace paperos
