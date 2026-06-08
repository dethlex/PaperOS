#include "SettingsApp.h"
#include "i18n/Strings.h"
#include "hal/Display.h"
#include "hal/Sd.h"
#include "hal/Wifi.h"
#include "hal/Rtc.h"
#include "services/ConfigStore.h"
#include "services/Storage.h"
#include "services/HAClient.h"
#include "services/WeatherService.h"
#include "services/WeatherData.h"
#include "framework/ui/Fonts.h"
#include "util/Logger.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <Arduino.h>

// Injected by tools/version_flag.py at build time (git describe). Fallback for
// native/IDE builds where the pre-build hook didn't run.
#ifndef PAPEROS_VERSION
#define PAPEROS_VERSION "dev"
#endif

namespace paperos {

void SettingsApp::enter(AppContext& ctx) {
    section_ = Section::Index;
    renderIndex(ctx);
}

void SettingsApp::renderIndex(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts; fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.setTextColor(15);
    c.drawString(tr(Str::app_settings), kHeaderTextX, kHeaderTextY);

    fonts.apply(c, FontFace::Serif, 26);
    const char* labels[] = {"WiFi", "Home Assistant", tr(Str::set_reading),
                             tr(Str::set_time), tr(Str::app_screensaver),
                             tr(Str::app_weather), tr(Str::set_language), tr(Str::set_about)};
    for (int i = 0; i < 8; ++i) {
        int16_t y = 100 + i * 100;
        c.drawRect(20, y, kScreenW - 40, 80, 12);
        c.drawString(labels[i], 40, y + 24);
    }
    ctx.display.pushRegion({0,0,kScreenW,kScreenH}, PushMode::Full);
}

void SettingsApp::onIndexTouch(int16_t x, int16_t y, AppContext& ctx) {
    (void)x;
    if (y < 100) return;
    int idx = (y - 100) / 100;
    if (idx < 0 || idx > 7) return;
    static const Section sections[] = {
        Section::Wifi, Section::Ha, Section::Reader,
        Section::Time, Section::Screensaver, Section::Weather, Section::Language, Section::About
    };
    section_ = sections[idx];
    rebuildRows(ctx);
    renderSection(ctx);
}

void SettingsApp::rebuildRows(AppContext& ctx) {
    rows_.clear();
    switch (section_) {
        case Section::Wifi:
            rows_.push_back({"SSID",   ctx.config.wifiSsid(), "wifi_ssid", false});
            rows_.push_back({tr(Str::set_password), ctx.config.wifiPass(), "wifi_pass", !show_wifi_pass_});
            rows_.push_back({show_wifi_pass_ ? tr(Str::set_hide_password) : tr(Str::set_show_password),
                             "", "wifi_show_toggle", false});
            rows_.push_back({tr(Str::set_test),   "",                    "wifi_test", false});
            break;
        case Section::Ha:
            rows_.push_back({"URL",       ctx.config.haUrl(),   "ha_url",   false});
            rows_.push_back({"Token",     ctx.config.haToken(), "ha_token", !show_ha_token_});
            rows_.push_back({show_ha_token_ ? tr(Str::set_hide_token) : tr(Str::set_show_token),
                             "", "ha_show_toggle", false});
            rows_.push_back({tr(Str::set_check), "",                   "ha_test",  false});
            break;
        case Section::Reader: {
            uint8_t fs = ctx.config.fontSize();
            const char* name = fs == 0 ? "S" : fs == 2 ? "L" : "M";
            rows_.push_back({tr(Str::set_font_size), name, "font_cycle", false});
            char m[8]; snprintf(m, sizeof(m), "%u px", ctx.config.marginPx());
            rows_.push_back({tr(Str::set_margin), m, "margin_cycle", false});
            break;
        }
        case Section::Time: {
            char tz[8]; snprintf(tz, sizeof(tz), "%+d", ctx.config.tzOffsetHours());
            rows_.push_back({"UTC offset", tz, "tz_cycle", false});
            rows_.push_back({tr(Str::set_sync_ntp), "", "ntp_now", false});
            break;
        }
        case Section::Screensaver: {
            char idle[16]; snprintf(idle, sizeof(idle), tr(Str::set_sec_fmt), ctx.config.screensaverIdleS());
            rows_.push_back({tr(Str::set_idle_to_saver), idle, "idle_cycle", false});
            char rot[16]; snprintf(rot, sizeof(rot), tr(Str::set_min_fmt), ctx.config.photoRotateMin());
            rows_.push_back({tr(Str::set_photo_rotate), rot, "rotate_cycle", false});
            break;
        }
        case Section::Weather: {
            rows_.push_back({tr(Str::set_lat),  ctx.config.weatherLat(), "wx_lat", false});
            rows_.push_back({tr(Str::set_lon), ctx.config.weatherLon(), "wx_lon", false});
            char rf[16]; snprintf(rf, sizeof(rf), tr(Str::set_min_fmt), ctx.config.weatherRefreshMin());
            rows_.push_back({tr(Str::set_refresh_interval), rf, "wx_refresh_cycle", false});
            char io[16]; snprintf(io, sizeof(io), "%+d\xC2\xB0", ctx.config.indoorTempOffset());
            rows_.push_back({tr(Str::set_temp_offset), io, "wx_offset_cycle", false});
            rows_.push_back({tr(Str::set_update_now), "", "wx_update", false});
            break;
        }
        case Section::Language: {
            // Active language is shown by a radio dot drawn in renderSection — a
            // font checkmark glyph (✓) isn't in DejaVuSerif and renders as tofu.
            rows_.push_back({"Русский", "", "lang_ru", false});
            rows_.push_back({"English", "", "lang_en", false});
            break;
        }
        case Section::About:
            rows_.push_back({tr(Str::set_version), PAPEROS_VERSION, "", false});
            rows_.push_back({tr(Str::set_sync_config), "", "config_sync", false});
            rows_.push_back({tr(Str::set_reboot), "", "reboot", false});
            break;
        default: break;
    }
}

void SettingsApp::renderSection(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts; fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.setTextColor(15);
    // Section title in the header. Going back is the hardware G38 button now —
    // no on-screen "Назад" button.
    const char* title;
    switch (section_) {
        case Section::Wifi:        title = "WiFi";           break;
        case Section::Ha:          title = "Home Assistant"; break;
        case Section::Reader:      title = tr(Str::set_reading);     break;
        case Section::Time:        title = tr(Str::set_time);        break;
        case Section::Screensaver: title = tr(Str::app_screensaver); break;
        case Section::Weather:     title = tr(Str::app_weather);     break;
        case Section::Language:    title = tr(Str::set_language);    break;
        case Section::About:       title = tr(Str::set_about);       break;
        default:                   title = tr(Str::app_settings);    break;
    }
    c.drawString(title, kHeaderTextX, kHeaderTextY);
    fonts.apply(c, FontFace::Serif, 24);
    int16_t y = 80;
    int ri = 0;
    for (const auto& r : rows_) {
        c.drawRect(20, y, kScreenW - 40, 70, 12);
        c.drawString(r.label.c_str(), 36, y + 12);
        if (section_ == Section::Language) {
            // Radio dot at the right edge: a ring always, filled when this row's
            // language (row 0 = ru, 1 = en) is active. Primitives only — never
            // depends on a font glyph (a ✓ would render as tofu in DejaVuSerif).
            int cx = kScreenW - 60, cy = y + 35;
            c.drawCircle(cx, cy, 16, 15);
            c.drawCircle(cx, cy, 15, 15);
            if (ctx.config.language() == ri) c.fillCircle(cx, cy, 9, 15);
        } else {
            std::string disp = r.mask ? std::string(r.value.size(), '*') : r.value;
            c.drawString(disp.c_str(), 36, y + 40);
        }
        y += 80;
        ++ri;
    }
    if (kb_.isOpen()) kb_.render(c);
    ctx.display.pushRegion({0,0,kScreenW,kScreenH}, PushMode::Full);
}

void SettingsApp::startEdit(const std::string& field_key, const std::string& current,
                            bool mask, AppContext& ctx) {
    editing_field_ = field_key;
    kb_.setMasked(mask);
    kb_.open(current, [this, &ctx](const std::string& v){ applyEdit(editing_field_, v, ctx); });
    renderSection(ctx);
}

void SettingsApp::applyEdit(const std::string& field_key, const std::string& v, AppContext& ctx) {
    if      (field_key == "wifi_ssid") ctx.config.setWifiSsid(v);
    else if (field_key == "wifi_pass") ctx.config.setWifiPass(v);
    else if (field_key == "ha_url")    ctx.config.setHaUrl(v);
    else if (field_key == "ha_token")  ctx.config.setHaToken(v);
    else if (field_key == "wx_lat")    ctx.config.setWeatherLat(v);
    else if (field_key == "wx_lon")    ctx.config.setWeatherLon(v);
    kb_.close();
    rebuildRows(ctx);
    renderSection(ctx);
}

void SettingsApp::wifiTest(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    Fonts fonts; fonts.apply(c, FontFace::Serif, 22);
    c.fillRect(20, kScreenH - 60, kScreenW - 40, 40, 0);
    c.drawString(tr(Str::set_connecting), 30, kScreenH - 55);
    ctx.display.pushRegion({20, kScreenH - 60, kScreenW - 40, 40}, PushMode::Quick);

    auto r = ctx.wifi.connect(ctx.config.wifiSsid(), ctx.config.wifiPass(), 8000);
    c.fillRect(20, kScreenH - 60, kScreenW - 40, 40, 0);
    char msg[128];
    if (r == WifiResult::Ok)
        snprintf(msg, sizeof(msg), "OK, IP=%s RSSI=%d", ctx.wifi.ipString().c_str(), ctx.wifi.rssi());
    else snprintf(msg, sizeof(msg), tr(Str::set_error_fmt), static_cast<int>(r));
    c.drawString(msg, 30, kScreenH - 55);
    ctx.display.pushRegion({20, kScreenH - 60, kScreenW - 40, 40}, PushMode::Quick);
    ctx.wifi.disconnect();
}

void SettingsApp::haTest(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    Fonts fonts; fonts.apply(c, FontFace::Serif, 22);
    // Status row spans two lines (the response can be longer than one) — use
    // 80 px of height instead of 40.
    auto status = [&](const char* line1, const char* line2) {
        c.fillRect(20, kScreenH - 90, kScreenW - 40, 80, 0);
        c.drawString(line1, 30, kScreenH - 85);
        if (line2 && *line2) c.drawString(line2, 30, kScreenH - 55);
        ctx.display.pushRegion({20, kScreenH - 90, kScreenW - 40, 80}, PushMode::Quick);
    };

    status(tr(Str::set_checking_ha), "");

    ctx.ha.begin();   // re-read url/token in case user just edited them
    if (!ctx.ha.ensureWifi()) {
        status(tr(Str::set_wifi_fail), "");
        return;
    }

    int code = 0;
    std::string err;
    bool ok = ctx.ha.ping(code, err);

    char l1[80], l2[96];
    if (ok) {
        snprintf(l1, sizeof(l1), tr(Str::set_ha_ok_fmt), code);
        l2[0] = 0;
    } else if (code == 0) {
        // Transport-layer error (TLS, DNS, timeout, no route, etc).
        snprintf(l1, sizeof(l1), "Transport error:");
        snprintf(l2, sizeof(l2), "%s", err.empty() ? "(unknown)" : err.c_str());
    } else if (code == 401) {
        snprintf(l1, sizeof(l1), "%s", tr(Str::set_auth_rejected));
        l2[0] = 0;
    } else if (code == 404) {
        snprintf(l1, sizeof(l1), "%s", tr(Str::set_endpoint_404));
        l2[0] = 0;
    } else {
        snprintf(l1, sizeof(l1), "HTTP %d", code);
        snprintf(l2, sizeof(l2), "%s", err.empty() ? "" : err.c_str());
    }
    status(l1, l2);
    ctx.ha.releaseWifi();
}

void SettingsApp::weatherUpdate(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    Fonts fonts; fonts.apply(c, FontFace::Serif, 22);
    auto toast = [&](const char* m) {
        c.fillRect(20, kScreenH - 60, kScreenW - 40, 40, 0);
        c.drawString(m, 30, kScreenH - 55);
        ctx.display.pushRegion({20, kScreenH - 60, kScreenW - 40, 40}, PushMode::Quick);
    };
    toast(tr(Str::set_updating_weather));
    ctx.weather.begin();
    if (!ctx.weather.ensureWifi()) { toast(tr(Str::set_wifi_fail)); return; }
    paperos::WeatherData wd;
    bool ok = ctx.weather.fetch(wd);
    if (ok) {
        wd.fetched_unix = static_cast<uint32_t>(ctx.rtc.nowUnix());
        ctx.weather.saveCache(wd);
    }
    ctx.weather.releaseWifi();
    char msg[64];
    if (ok) snprintf(msg, sizeof(msg), tr(Str::set_wx_ok_fmt), wd.current_c, wd.day_count);
    else    snprintf(msg, sizeof(msg), "%s", tr(Str::set_wx_fail));
    toast(msg);
}


void SettingsApp::onSectionTouch(int16_t x, int16_t y, AppContext& ctx) {
    if (kb_.isOpen()) {
        // Forward to keyboard first. Keyboard::onTouch returns false when the
        // tap is above kKbTop — i.e. the user tapped the section UI (a row)
        // while the keyboard was still open. Treat that as a dismiss: close
        // the keyboard and re-render. User can tap again to act on the row.
        if (kb_.onTouch(x, y, ctx.display)) return;
        kb_.close();
        renderSection(ctx);
        return;
    }
    if (y < 80) return;   // header row (title); back is the G38 button
    int idx = (y - 80) / 80;
    if (idx < 0 || idx >= static_cast<int>(rows_.size())) return;
    const Row& r = rows_[idx];
    if (r.field_key.empty()) return;
    if      (r.field_key == "wifi_ssid" || r.field_key == "wifi_pass" ||
             r.field_key == "ha_url"    || r.field_key == "ha_token"  ||
             r.field_key == "wx_lat"    || r.field_key == "wx_lon") {
        startEdit(r.field_key, r.value, r.mask, ctx);
    }
    else if (r.field_key == "wifi_test") { wifiTest(ctx); }
    else if (r.field_key == "ha_test")   { haTest(ctx); }
    else if (r.field_key == "wifi_show_toggle") {
        show_wifi_pass_ = !show_wifi_pass_;
        rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "ha_show_toggle") {
        show_ha_token_ = !show_ha_token_;
        rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "font_cycle"){
        uint8_t v = (ctx.config.fontSize() + 1) % 3; ctx.config.setFontSize(v);
        rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "margin_cycle"){
        uint8_t cur = ctx.config.marginPx();
        uint8_t next = cur == 16 ? 24 : cur == 24 ? 32 : 16;
        ctx.config.setMarginPx(next); rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "tz_cycle"){
        int8_t v = ctx.config.tzOffsetHours() + 1; if (v > 12) v = -11;
        ctx.config.setTzOffsetHours(v); rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "idle_cycle"){
        uint16_t cur = ctx.config.screensaverIdleS();
        uint16_t next = cur == 120 ? 300 : cur == 300 ? 600 : cur == 600 ? 1800 : 120;
        ctx.config.setScreensaverIdleS(next); rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "rotate_cycle"){
        uint16_t cur = ctx.config.photoRotateMin();
        uint16_t next = cur == 15 ? 30 : cur == 30 ? 60 : 15;
        ctx.config.setPhotoRotateMin(next); rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "wx_refresh_cycle") {
        uint16_t cur = ctx.config.weatherRefreshMin();
        uint16_t next = cur == 15 ? 30 : cur == 30 ? 60 : 15;
        ctx.config.setWeatherRefreshMin(next);   // tick path reads this from NVS
        rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "wx_offset_cycle") {
        int8_t cur = ctx.config.indoorTempOffset();
        // Negative-only by design: the SHT30 self-heats (reads high), so the
        // realistic correction is downward. Cycle 0→-1→-2→-3→-4→0; an imported
        // positive offset (config.json) is honoured but resets to 0 on first tap.
        int8_t next = (cur <= -4 || cur > 0) ? 0 : static_cast<int8_t>(cur - 1);
        ctx.config.setIndoorTempOffset(next);
        rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "wx_update") { weatherUpdate(ctx); }
    else if (r.field_key == "lang_ru") {
        ctx.config.setLanguage(0); i18nSetLang(Lang::Ru);
        rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "lang_en") {
        ctx.config.setLanguage(1); i18nSetLang(Lang::En);
        rebuildRows(ctx); renderSection(ctx);
    }
    else if (r.field_key == "ntp_now") {
        auto& c = ctx.display.canvas();
        Fonts fontsNtp; fontsNtp.apply(c, FontFace::Serif, 22);
        c.fillRect(20, kScreenH - 60, kScreenW - 40, 40, 0);
        c.drawString("NTP...", 30, kScreenH - 55);
        ctx.display.pushRegion({20, kScreenH - 60, kScreenW - 40, 40}, PushMode::Quick);
        bool ok = false;
        if (ctx.wifi.connect(ctx.config.wifiSsid(), ctx.config.wifiPass(), 8000) == WifiResult::Ok) {
            ok = ctx.rtc.syncFromNtp(ctx.config.tzOffsetHours());
            ctx.wifi.disconnect();
        }
        c.fillRect(20, kScreenH - 60, kScreenW - 40, 40, 0);
        c.drawString(ok ? tr(Str::set_time_updated) : tr(Str::set_ntp_fail), 30, kScreenH - 55);
        ctx.display.pushRegion({20, kScreenH - 60, kScreenW - 40, 40}, PushMode::Quick);
    }
    else if (r.field_key == "config_sync") {
        auto& c2 = ctx.display.canvas();
        Fonts fSync; fSync.apply(c2, FontFace::Serif, 22);
        auto toastSync = [&](const char* m) {
            c2.fillRect(20, kScreenH - 60, kScreenW - 40, 40, 0);
            c2.drawString(m, 30, kScreenH - 55);
            ctx.display.pushRegion({20, kScreenH - 60, kScreenW - 40, 40}, PushMode::Quick);
        };
        if (!ctx.sd.present()) toastSync(tr(Str::common_no_sd));
        else                   toastSync(ctx.config.syncWithFile() ? tr(Str::set_synced) : tr(Str::set_json_error));
        rebuildRows(ctx); renderSection(ctx);   // значения могли измениться из файла
    }
    else if (r.field_key == "reboot")           { ESP.restart(); }
}

void SettingsApp::onInput(const InputEvent& e, AppContext& ctx) {
    // Settings content fits on screen in every section, so NavUp/NavDown are
    // intentional no-ops here (nothing to scroll). Only touch is handled.
    if (e.kind != InputEvent::Kind::TouchUp) return;
    if (section_ == Section::Index) onIndexTouch(e.x, e.y, ctx);
    else                            onSectionTouch(e.x, e.y, ctx);
}

bool SettingsApp::onBack(AppContext& ctx) {
    if (kb_.isOpen()) {                 // close the keyboard first
        kb_.close();
        renderSection(ctx);
        return true;
    }
    if (section_ != Section::Index) {   // a section → back to the index
        section_ = Section::Index;
        renderIndex(ctx);
        return true;
    }
    return false;                       // index is the top level → Launcher
}

} // namespace paperos
