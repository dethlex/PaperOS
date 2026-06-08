#include "WeatherApp.h"
#include "hal/Display.h"
#include "hal/Rtc.h"
#include "services/WeatherService.h"
#include "hal/Sht30.h"
#include "services/ConfigStore.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/IconKit.h"
#include "util/WmoCode.h"
#include "i18n/Strings.h"
#include <Arduino.h>
#include <cstdio>

namespace paperos {

void WeatherApp::enter(AppContext& ctx) {
    ctx.weather.begin();
    indoor_ = ctx.sht30.read(ctx.config.indoorTempOffset());
    offline_ = false;
    ctx.weather.loadCache(data_);   // instant; data_.valid may be false
    render(ctx);                    // show cached (or empty) immediately
    refresh(ctx);                   // then go online
}

void WeatherApp::leave(AppContext& ctx) {
    ctx.weather.releaseWifi();
}

void WeatherApp::onInput(const InputEvent& e, AppContext& ctx) {
    // Tap = manual refresh, with a cooldown so a fat-finger / repeated TouchUp
    // doesn't fire several blocking ~8s WiFi fetches back to back.
    // NavUp/NavDown are no-ops (everything fits on one screen).
    if (e.kind != InputEvent::Kind::TouchUp) return;
    if (millis() - last_refresh_ms_ < 10000) return;
    refresh(ctx);
}

void WeatherApp::refresh(AppContext& ctx) {
    indoor_ = ctx.sht30.read(ctx.config.indoorTempOffset());
    offline_ = !ctx.weather.ensureWifi();
    if (!offline_) {
        WeatherData wd;
        if (ctx.weather.fetch(wd)) {
            wd.fetched_unix = static_cast<uint32_t>(ctx.rtc.nowUnix());
            data_ = wd;
            ctx.weather.saveCache(wd);
        }
    }
    render(ctx);
    last_refresh_ms_ = millis();
}

void WeatherApp::render(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.setTextColor(15);
    c.drawString(tr(Str::app_weather), kHeaderTextX, kHeaderTextY);
    if (offline_) {
        fonts.apply(c, FontFace::Serif, 18);
        c.drawString(tr(Str::weather_offline), kScreenW - 110, kHeaderTextY + 8);
    }

    if (!data_.valid) {
        fonts.apply(c, FontFace::Serif, 24);
        c.drawString(tr(Str::weather_no_data), 20, kContentTopY);
        ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
        return;
    }

    // Outdoor + indoor as symmetric stacked rows: [icon | temp | 💧humidity].
    // Same columns on both rows; outdoor keeps a small condition label at the
    // right, indoor is icons-only.
    WeatherCat cat = catFromWmo(data_.current_wmo);
    char t[16];

    // Outdoor row.
    IconKit::weather(c, cat, 30, 80, 72);
    snprintf(t, sizeof(t), "%+d\xC2\xB0", data_.current_c);   // \xC2\xB0 = ° (UTF-8)
    fonts.apply(c, FontFace::Serif, 56);
    c.drawString(t, 118, 86);
    IconKit::drop(c, 250, 86, 40);
    fonts.apply(c, FontFace::Serif, 30);
    char eh[8]; snprintf(eh, sizeof(eh), "%u%%", data_.current_humidity);
    c.drawString(eh, 296, 92);
    fonts.apply(c, FontFace::Serif, 22);
    c.drawString(tr(wmoLabelId(cat)), 372, 94);

    // Indoor row (same columns; house icon = "in the room").
    IconKit::house(c, 30, 168, 72);
    fonts.apply(c, FontFace::Serif, 56);
    if (indoor_.valid) {
        char it[16]; snprintf(it, sizeof(it), "%+d\xC2\xB0", indoor_.temp_c);
        c.drawString(it, 118, 174);
        IconKit::drop(c, 250, 174, 40);
        fonts.apply(c, FontFace::Serif, 30);
        char ih[8]; snprintf(ih, sizeof(ih), "%u%%", indoor_.humidity);
        c.drawString(ih, 296, 180);
    } else {
        c.drawString("--", 118, 174);
    }

    // Hourly strip (next ~12h): hour / small icon / temp.
    int16_t y = 252;
    c.drawLine(20, y, kScreenW - 20, y, 8);
    {
        const int n = data_.hour_count;
        const int x0 = 16, colw = (kScreenW - 32) / 12;   // 12 columns max
        fonts.apply(c, FontFace::Serif, 18);
        for (int i = 0; i < n && i < 12; ++i) {
            const HourForecast& h = data_.hours[i];
            int hx = x0 + i * colw;
            char hh[4]; snprintf(hh, sizeof(hh), "%02u", h.hour);
            c.drawString(hh, hx + 4, y + 8);
            IconKit::weather(c, catFromWmo(h.wmo_code), hx + 6, y + 30, 24);
            char ht[12]; snprintf(ht, sizeof(ht), "%d\xC2\xB0", h.temp_c);
            c.drawString(ht, hx + 2, y + 60);
        }
    }

    // 5-day forecast list.
    y = 344;
    c.drawLine(20, y, kScreenW - 20, y, 8);
    y += 16;
    for (uint8_t i = 0; i < data_.day_count; ++i) {
        const DayForecast& d = data_.days[i];
        IconKit::weather(c, catFromWmo(d.wmo_code), 30, y, 52);
        fonts.apply(c, FontFace::Serif, 30);
        // d.weekday is Mon-first (0=Mon); i18n daysShort() is Sun-first → remap.
        c.drawString(d.weekday < 7 ? daysShort()[(d.weekday + 1) % 7] : "—", 110, y + 12);
        char hl[28];
        snprintf(hl, sizeof(hl), "%+d\xC2\xB0 / %+d\xC2\xB0", d.tmax_c, d.tmin_c);
        c.drawString(hl, 210, y + 12);
        y += 72;
    }

    // "Обновлено HH:MM" — full-width footer below the forecast (avoids the
    // header right-edge clip). Shown whenever we have a fetch timestamp, even
    // offline (then it's the last-known cache time). Local wall-clock via
    // formatHHMMUnix.
    if (data_.fetched_unix != 0) {
        char hhmm[8]; ctx.rtc.formatHHMMUnix(data_.fetched_unix, hhmm, sizeof(hhmm));
        char upd[28]; snprintf(upd, sizeof(upd), tr(Str::weather_updated_fmt), hhmm);
        fonts.apply(c, FontFace::Serif, 20);
        c.drawString(upd, 20, y + 8);
    }
    ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
}

} // namespace paperos
