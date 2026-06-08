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

    esp_light_sleep_start();                            // returns on wake

    last_wake_ = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO)
                 ? WakeCause::Button : WakeCause::Timer;
    gpio_wakeup_disable(kBtnG38);                       // disarm until next sleep
    LOG_INFO("power", "wake: %s", last_wake_ == WakeCause::Button ? "G38" : "timer");

    display_.wake();                                    // restore IT8951 (~2s settle)
}

} // namespace paperos
