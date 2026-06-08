#pragma once
#include <stdint.h>
#include "hal/Display.h"

namespace paperos {

enum class WakeCause : uint8_t { Timer, Button };

class Power {
public:
    explicit Power(Display& d) : display_(d) {}

    // Low-power LIGHT sleep. Unlike esp_deep_sleep, the digital core stays
    // powered, so the M5Paper main-power latch (GPIO2, driven HIGH) keeps
    // holding and the board survives on battery (deep sleep drops it → the
    // board dies — the original "lock screen freezes on battery" bug). Wakes
    // after `seconds` (clock tick) or on a G38 press; execution RESUMES in
    // place (no reboot, RAM/peripherals retained). Query lastWake() after.
    void lightSleep(uint32_t seconds);
    WakeCause lastWake() const { return last_wake_; }

private:
    Display& display_;
    WakeCause last_wake_ = WakeCause::Timer;
};

} // namespace paperos
