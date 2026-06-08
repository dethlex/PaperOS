#include <Arduino.h>
#include <M5EPD.h>
#include <esp_log.h>
#include <esp_core_dump.h>

#include "hal/Display.h"
#include "hal/Touch.h"
#include "hal/Buttons.h"
#include "hal/Rtc.h"
#include "hal/Sd.h"
#include "hal/Nvs.h"
#include "hal/Wifi.h"
#include "hal/Sht30.h"
#include "hal/Battery.h"

#include "services/Power.h"
#include "services/Storage.h"
#include "services/ConfigStore.h"
#include "services/BookIndex.h"
#include "services/HAClient.h"
#include "services/WeatherService.h"
#include "services/WeatherData.h"
#include "services/WebDavServer.h"

#include "framework/AppSwitcher.h"
#include "framework/InputRouter.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/Geometry.h"
#include "i18n/Strings.h"

#include "apps/launcher/Launcher.h"
#include "apps/reader/Reader.h"
#include "apps/ha/HomeAssistantApp.h"
#include "apps/settings/SettingsApp.h"
#include "apps/screensaver/Screensaver.h"
#include "apps/weather/WeatherApp.h"
#include "apps/fileserver/FileServerApp.h"
#include "apps/games/GamesApp.h"

#include "util/Logger.h"

static paperos::Display    g_display;
static paperos::Touch      g_touch;
static paperos::Buttons    g_buttons;
static paperos::Rtc        g_rtc;
static paperos::Sd         g_sd;
static paperos::Nvs        g_nvs;
static paperos::Wifi       g_wifi;
static paperos::Sht30      g_sht30;
static paperos::Battery    g_battery;
static paperos::Power      g_power_inst(g_display);
static paperos::Storage    g_storage(g_sd);
static paperos::ConfigStore g_config(g_nvs, g_sd);
static paperos::BookIndex  g_books(g_sd);
static paperos::HAClient   g_ha(g_wifi, g_config);
static paperos::WeatherService g_weather(g_wifi, g_config);
static paperos::WebDavServer g_webdav(g_sd);
static paperos::AppSwitcher g_switcher;
static paperos::InputRouter g_router(g_touch, g_buttons, g_switcher);

static paperos::AppContext g_ctx{
    g_display, g_config, g_storage, g_power_inst,
    g_sd, g_wifi, g_rtc, g_books, g_ha, g_switcher, g_weather, g_sht30, g_battery, g_webdav
};

// One-shot boot refresh. The BM8563 RTC has no backup battery on M5Paper — it loses
// time on any full power-cycle / reflash (the 3V3 rail blips while the firmware isn't
// driving the GPIO2 latch), so the clock comes up wrong. If WiFi is configured we
// briefly bring it up to NTP-sync the clock and refresh the weather cache, then drop
// WiFi. No SSID configured → return instantly (a WiFi-less unit boots without delay).
// Runs before switching to the first app so the UI opens already-fresh; shows a splash
// because it blocks a few seconds.
static void bootSync(paperos::AppContext& ctx) {
    if (ctx.config.wifiSsid().empty()) return;

    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    paperos::Fonts fonts;
    fonts.apply(c, paperos::FontFace::Serif, 30);
    c.setTextColor(15);
    c.drawString(paperos::tr(paperos::Str::boot_loading), paperos::kScreenW / 2 - 80, paperos::kScreenH / 2 - 16);
    ctx.display.pushRegion({0, 0, paperos::kScreenW, paperos::kScreenH}, paperos::PushMode::Full);

    if (ctx.wifi.connect(ctx.config.wifiSsid(), ctx.config.wifiPass(), 8000) == paperos::WifiResult::Ok) {
        ctx.rtc.syncFromNtp(ctx.config.tzOffsetHours());     // best-effort; clock stays as-is on failure
        ctx.weather.begin();
        paperos::WeatherData wd;
        if (ctx.weather.fetch(wd)) {
            wd.fetched_unix = static_cast<uint32_t>(ctx.rtc.nowUnix());
            ctx.weather.saveCache(wd);
        }
    }
    ctx.wifi.disconnect();
}

void setup() {
    // M5.begin() with defaults handles SerialEnable+SDEnable+touchEnable+ADC+I2C in
    // the proper SPI/I2C bring-up order — matches the upstream M5EPD example.
    // Silence sd_diskio / vfs_api error spam from M5.begin() when no SD card
    // is inserted — Sd::begin() handles the absent-card case via SD.cardType().
    esp_log_level_set("sd_diskio", ESP_LOG_NONE);
    esp_log_level_set("vfs_api",   ESP_LOG_NONE);

    M5.begin();
    LOG_INFO("main", "boot");

    // Flush Serial on any clean shutdown (e.g. long-press power-off) so the
    // last log lines aren't lost in the UART FIFO.
    esp_register_shutdown_handler([](){ Serial.flush(); });

    // Detect a coredump left by a previous crash. We only log it for now;
    // surfacing in Settings → About is a V2 follow-up.
    {
        size_t addr = 0, len = 0;
        if (esp_core_dump_image_get(&addr, &len) == ESP_OK) {
            LOG_WARN("main", "previous coredump present at 0x%x, len=%u",
                     static_cast<unsigned>(addr), static_cast<unsigned>(len));
        }
    }

    g_display.begin(/*full_init=*/true);
    g_rtc.begin();
    g_sd.begin();  // sets ok_ flag — M5.begin() already did the SPI/SD init

    // Always a full boot. We no longer reboot to tick the clock — the screensaver
    // light-sleeps in place and resumes (see Screensaver::enter). Light sleep
    // keeps the board powered (GPIO2 main-power latch holds), so on battery the
    // device survives sleep and a G38 press wakes it back into the UI. (Full
    // power-off / esp_deep_sleep would kill the rail on battery — the old bug —
    // and this unit has no power button to recover.)
    g_touch.begin();
    g_sht30.begin();
    g_battery.begin();
    g_buttons.begin();
    g_nvs.begin();
    g_wifi.disconnect();                              // ensure off until needed
    g_storage.ensureLayout();

    g_switcher.registerApp("launcher",    [](){ return static_cast<paperos::IApp*>(new paperos::Launcher()); });
    g_switcher.registerApp("reader",      [](){ return static_cast<paperos::IApp*>(new paperos::Reader()); });
    g_switcher.registerApp("ha",          [](){ return static_cast<paperos::IApp*>(new paperos::HomeAssistantApp()); });
    g_switcher.registerApp("settings",    [](){ return static_cast<paperos::IApp*>(new paperos::SettingsApp()); });
    g_switcher.registerApp("weather",     [](){ return static_cast<paperos::IApp*>(new paperos::WeatherApp()); });
    g_switcher.registerApp("screensaver", [](){ return static_cast<paperos::IApp*>(new paperos::Screensaver()); });
    g_switcher.registerApp("fileserver",  [](){ return static_cast<paperos::IApp*>(new paperos::FileServerApp()); });
    g_switcher.registerApp("games",       [](){ return static_cast<paperos::IApp*>(new paperos::GamesApp()); });
    g_switcher.setContext(&g_ctx);

    {
        uint32_t idle_ms = g_config.screensaverIdleS() * 1000UL;
        if (idle_ms < 30000UL) idle_ms = 300000UL;   // sanity: never fire idle in <30s
        g_router.setIdleTimeoutMs(idle_ms);
        g_router.resetIdle();                          // start the timer from "now", not millis()=0
    }

    // Pull config.json from the card into NVS (and write the full config back) BEFORE
    // bootSync, so WiFi creds from the file apply before WiFi comes up at boot.
    // No-op without SD. External edits to config.json (incl. over WebDAV) are picked up here.
    g_config.syncWithFile();
    paperos::i18nSetLang(static_cast<paperos::Lang>(g_config.language()));

    // Boot refresh: NTP-sync the clock + refresh weather over WiFi (no-op without a
    // configured SSID). Done before the first render so the UI opens with correct
    // time and fresh weather (BM8563 loses time across a reflash/power-cycle).
    bootSync(g_ctx);

    // Manual boot (power button / cold boot): restore the last app the user was
    // in (saved to NVS before sleeping), falling back to the launcher.
    std::string id = g_switcher.idOfIndex(g_config.lastAppIndex());
    if (id.empty() || id == "screensaver") id = "launcher";
    g_switcher.switchTo(id);
    Serial.println("[paperos] switchTo done");
}

void loop() {
    M5.update();
    bool idle = g_router.tick(g_ctx);
    // No hard power-off on long-press: this unit has no power button, so a full
    // shutdown would strand it (only USB could revive it). The screensaver's
    // light-sleep is the low-power state; short G38 locks to it.

    if (idle) {
        if (auto* a = g_switcher.current(); a && a->keepAwake()) {
            // App asks to stay awake (serving files) — extend the idle window.
            g_router.resetIdle();
        } else {
            // Save last app index to NVS before switching; switchTo() handles leave()
            // itself, so calling a->leave() here would double-fire it.
            if (auto* a = g_switcher.current()) {
                g_config.setLastAppIndex(static_cast<uint8_t>(g_switcher.indexOf(a->id())));
            }
            g_switcher.switchTo("screensaver");          // blocks in the lock-screen
                                                         // light-sleep loop until G38
            // Woke from the lock screen: the GT911 may still hold a tap the user
            // made while we slept. Drain it and suppress touch briefly so it isn't
            // replayed to the app we wake into (ghost-opening a launcher tile).
            g_touch.drain();
            g_router.suppressTouchAfterWake();
        }
    }

    if (g_switcher.current()) g_switcher.current()->tick(g_ctx);
    if (g_switcher.hasPending()) {
        g_switcher.applyPending();
        // Start a fresh idle window after any switch — in particular when waking
        // from the screensaver (G38), so we don't immediately idle back to sleep.
        g_router.resetIdle();
    }

    delay(20);
}
