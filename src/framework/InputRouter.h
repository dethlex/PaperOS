#pragma once
#include "App.h"
#include "hal/Touch.h"
#include "hal/Buttons.h"
#include <Arduino.h>

namespace paperos {

class AppSwitcher;

class InputRouter {
public:
    InputRouter(Touch& t, Buttons& b, AppSwitcher& s) : touch_(t), btn_(b), switcher_(s) {}

    void setIdleTimeoutMs(uint32_t ms) { idle_timeout_ms_ = ms; }
    void resetIdle() { last_activity_ms_ = millis(); idle_emitted_ = false; }

    // Briefly ignore touch input (and forget any latched press state). Called
    // after waking from the light-sleep lock screen: the GT911 still holds a tap
    // the user made while the CPU slept, and without this it gets replayed to the
    // app we wake into — "ghost-opening" the launcher tile under the tap point.
    void suppressTouchAfterWake(uint32_t ms = 700) {
        suppress_touch_until_ms_ = millis() + ms;
        prev_pressed_ = false;
    }

    // Pulls input and dispatches to current app. Returns true if an Idle event was emitted this tick.
    bool tick(AppContext& ctx);

    // G38 held ≥ kLongPressMs → shutdown (checked by main.cpp loop).
    bool homeLongPressed() const { return g38_held_since_ms_ != 0 && (millis() - g38_held_since_ms_) >= kLongPressMs; }

    static constexpr uint32_t kLongPressMs      = 3000;
    static constexpr uint32_t kDebounceMs       = 5;
    // E-ink refresh waveforms (GL16/A2/GC16) electromagnetically perturb the
    // GT911 touch controller and it occasionally emits a spurious press
    // immediately after a real release. Ignore TouchDown events that arrive
    // within this window after a TouchUp — empirically ~200 ms catches the
    // noise without making rapid intentional taps feel sluggish.
    static constexpr uint32_t kTouchDebounceMs  = 200;

private:
    Touch& touch_;
    Buttons& btn_;
    AppSwitcher& switcher_;

    uint32_t idle_timeout_ms_ = 300 * 1000;
    uint32_t last_activity_ms_ = 0;
    bool idle_emitted_ = false;

    bool prev_pressed_ = false;
    bool prev_btn_[3] = {false,false,false};
    uint32_t prev_btn_change_[3] = {0,0,0};
    Point last_touch_at_{0,0};
    uint32_t last_touch_up_ms_ = 0;       // for kTouchDebounceMs window after release
    uint32_t suppress_touch_until_ms_ = 0; // post-wake touch-suppression deadline (0 = inactive)

    uint32_t g38_held_since_ms_ = 0;   // for G38 short(back)/long(shutdown)
};

} // namespace paperos
