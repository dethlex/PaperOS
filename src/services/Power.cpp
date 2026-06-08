#include "Power.h"
#include "util/Logger.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

namespace paperos {

// G38 (M5EPD_KEY_PUSH_PIN) — active-low with an external pull-up: HIGH at rest,
// LOW when pressed. We wake the light sleep on its LOW level.
static constexpr gpio_num_t kBtnG38 = GPIO_NUM_38;

void Power::lightSleep(uint32_t seconds) {
    // Wait for any in-flight e-ink waveform, THEN cut the IT8951 EXT power so the
    // controller draws ~nothing during sleep. The GPIO2 main-power latch is a
    // separate pin (GPIO5 vs GPIO2), so the board stays alive on battery and the
    // e-ink panel physically retains its frame. We re-init the controller via
    // display_.wake() right after esp_light_sleep_start() returns, before the
    // caller renders anything.
    display_.flush();
    display_.powerDown();                               // cut IT8951 (GPIO5)

    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
    gpio_wakeup_enable(kBtnG38, GPIO_INTR_LOW_LEVEL);   // G38 press → wake
    esp_sleep_enable_gpio_wakeup();                     // GPIO wake (light sleep)

    uint32_t t0 = millis();
    esp_err_t rc = esp_light_sleep_start();             // returns on wake
    uint32_t slept_ms = millis() - t0;
    gpio_wakeup_disable(kBtnG38);                       // disarm until next sleep

    if (rc != ESP_OK) {
        // Sleep entry was REJECTED (e.g. a wake source already satisfied at the
        // instant of entry). Defensive: without this the screensaver while-loop
        // would busy-spin at full power re-initing the EPD ~continuously. Fall
        // back to a real blocking wait for the requested interval and surface it.
        last_wake_ = WakeCause::Timer;
        LOG_WARN("power", "light sleep REJECTED rc=%d — fallback delay %us",
                 static_cast<int>(rc), static_cast<unsigned>(seconds));
        delay(static_cast<uint32_t>(seconds) * 1000UL);
    } else {
        last_wake_ = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO)
                     ? WakeCause::Button : WakeCause::Timer;
    }
    // The "slept" duration is the on-device drain diagnostic: a healthy lock
    // screen logs one "wake: timer (slept ~60000ms)" per minute. Many lines per
    // second ⇒ sleep is not engaging (look here first).
    LOG_INFO("power", "wake: %s (slept %ums)",
             last_wake_ == WakeCause::Button ? "G38" : "timer",
             static_cast<unsigned>(slept_ms));

    display_.wake();                                    // restore IT8951 (~2s settle)
}

} // namespace paperos
