#pragma once
#include <M5EPD.h>
#include "framework/ui/Geometry.h"

namespace paperos {

struct TouchSample {
    bool pressed;
    Point at;
};

class Touch {
public:
    void begin() {
        // GT911 native orientation is landscape (960x540). Match the EPD
        // rotation so touch coords align with screen pixels.
        M5.TP.SetRotation(90);
    }
    // Discard any latched/pending touch in the GT911. Used after waking from the
    // light-sleep lock screen: a tap made while the CPU slept stays latched in the
    // controller and would otherwise be replayed to the app we wake into. A few
    // update() calls consume the buffered event and clear the INT.
    void drain() {
        for (int i = 0; i < 3; ++i) M5.TP.update();
    }

    // Returns true and fills `out` if a touch is currently active.
    // Returns false otherwise. Coordinates are screen-relative (rotation 90).
    bool poll(TouchSample& out) {
        if (!M5.TP.available()) return false;
        // GT911 quirk: isFingerUp() returns a flag set by update(); calling it
        // first reads stale data from the previous event. Always update first.
        M5.TP.update();
        if (M5.TP.isFingerUp()) { out.pressed = false; return true; }
        tp_finger_t f = M5.TP.readFinger(0);
        out.pressed = true;
        out.at = { static_cast<int16_t>(f.x), static_cast<int16_t>(f.y) };
        return true;
    }
};

} // namespace paperos
