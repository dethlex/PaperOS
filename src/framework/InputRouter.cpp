#include "InputRouter.h"
#include "AppSwitcher.h"
#include "util/Logger.h"
#include <cstring>

namespace paperos {

static InputEvent makeEvent(InputEvent::Kind k, int16_t x, int16_t y, uint8_t btn, uint32_t now) {
    InputEvent e;
    e.kind    = k;
    e.x       = x;
    e.y       = y;
    e.button  = btn;
    e.now_ms  = now;
    return e;
}

bool InputRouter::tick(AppContext& ctx) {
    uint32_t now = millis();
    IApp* app = switcher_.current();

    // Touch
    TouchSample ts;
    if (touch_.poll(ts)) {
        // Post-wake suppression: right after the lock screen wakes (G38), the
        // GT911 may still hold a tap the user made while we light-slept. Swallow
        // touches until the window elapses and keep prev_pressed_ clear so no
        // stale TouchUp is synthesised — otherwise that tap "ghost-opens" the
        // launcher tile beneath it.
        bool suppress = suppress_touch_until_ms_ != 0 &&
                        static_cast<int32_t>(now - suppress_touch_until_ms_) < 0;
        if (suppress_touch_until_ms_ != 0 && !suppress) suppress_touch_until_ms_ = 0;  // window elapsed
        if (suppress) {
            prev_pressed_ = false;
        } else {
        // Phantom-touch suppression: GT911 sometimes fires a spurious press
        // shortly after a real release while the e-ink waveform is still
        // running. Drop any TouchDown that arrives within kTouchDebounceMs of
        // the last TouchUp. Active drags (prev_pressed_ already true) bypass
        // the gate so a real long-press isn't cut.
        const bool in_debounce = (now - last_touch_up_ms_) < kTouchDebounceMs;
        if (ts.pressed && !prev_pressed_ && in_debounce) {
            // swallow — keep prev_pressed_ false so we don't latch onto noise
        } else if (ts.pressed && !prev_pressed_) {
            InputEvent e = makeEvent(InputEvent::Kind::TouchDown, ts.at.x, ts.at.y, 0, now);
            if (app) app->onInput(e, ctx);
            prev_pressed_ = true; last_touch_at_ = ts.at; resetIdle();
        } else if (ts.pressed && prev_pressed_ &&
                   (ts.at.x != last_touch_at_.x || ts.at.y != last_touch_at_.y)) {
            InputEvent e = makeEvent(InputEvent::Kind::TouchMove, ts.at.x, ts.at.y, 0, now);
            if (app) app->onInput(e, ctx);
            last_touch_at_ = ts.at; resetIdle();
        } else if (!ts.pressed && prev_pressed_) {
            InputEvent e = makeEvent(InputEvent::Kind::TouchUp, last_touch_at_.x, last_touch_at_.y, 0, now);
            if (app) app->onInput(e, ctx);
            prev_pressed_ = false;
            last_touch_up_ms_ = now;
            resetIdle();
        }
        }   // end of post-wake-suppression else
    }

    // Buttons (debounced). New scheme:
    //   G37 (ButtonId::Down)  → NavUp   (app-dependent: page back / scroll up)
    //   G39 (ButtonId::Up)    → NavDown (app-dependent: page fwd / scroll down)
    //   G38 (ButtonId::Mid)   → short = back, long(≥3s) = shutdown. Captured here.
    bool lock_requested = false;
    for (uint8_t i = 0; i < 3; ++i) {
        ButtonId id = static_cast<ButtonId>(i);
        bool now_pressed = btn_.pressed(id);
        if (now_pressed == prev_btn_[i] || now - prev_btn_change_[i] < kDebounceMs) continue;
        prev_btn_change_[i] = now;
        prev_btn_[i] = now_pressed;

        if (id == ButtonId::Mid) {                 // G38 — back / lock / shutdown
            if (now_pressed) {
                g38_held_since_ms_ = now;
            } else {
                bool was_long = g38_held_since_ms_ && (now - g38_held_since_ms_) >= kLongPressMs;
                g38_held_since_ms_ = 0;
                if (!was_long) {                    // short press → "back"
                    IApp* cur = switcher_.current();
                    bool handled = cur && cur->onBack(ctx);
                    if (!handled) {
                        const char* cur_id = cur ? cur->id() : "";
                        if (std::strcmp(cur_id, "launcher") == 0) {
                            lock_requested = true;  // already home → lock screen
                        } else {
                            switcher_.requestSwitch("launcher");
                        }
                    }
                }
                // long press: shutdown is triggered by main.cpp via
                // homeLongPressed() while still held; nothing to do on release.
            }
            resetIdle();
            continue;
        }

        // G37 → NavUp, G39 → NavDown. Fire once on the press edge only
        // (no auto-repeat — e-ink can't keep up with rapid refreshes).
        if (now_pressed) {
            InputEvent::Kind k = (id == ButtonId::Down) ? InputEvent::Kind::NavUp
                                                        : InputEvent::Kind::NavDown;
            InputEvent e = makeEvent(k, 0, 0, i, now);
            if (app) app->onInput(e, ctx);
        }
        resetIdle();
    }
    if (lock_requested) return true;   // funnel through main.cpp's screensaver path

    // Idle detection. Re-read millis() AFTER input handling so resetIdle() during
    // this tick can't push `last_activity_ms_` past our cached `now` and cause
    // a uint32_t underflow that looks like a 49-day-old timestamp.
    const uint32_t idle_now = millis();
    const int32_t  delta    = static_cast<int32_t>(idle_now - last_activity_ms_);
    if (!idle_emitted_ && delta >= static_cast<int32_t>(idle_timeout_ms_)) {
        InputEvent e = makeEvent(InputEvent::Kind::Idle, 0, 0, 0, idle_now);
        if (app) app->onInput(e, ctx);
        idle_emitted_ = true;
        return true;
    }
    return false;
}

} // namespace paperos
