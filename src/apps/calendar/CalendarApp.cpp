#include "CalendarApp.h"
#include "hal/Display.h"
#include "hal/Rtc.h"
#include "services/CalendarService.h"
#include "services/ConfigStore.h"
#include "framework/AppSwitcher.h"
#include "framework/ui/Fonts.h"
#include "i18n/Strings.h"
#include "util/Utf8.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace paperos {

void CalendarApp::enter(AppContext& ctx) {
    ctx.calendar.begin();
    configured_ = ctx.calendar.configured();
    offline_ = false;
    ctx.calendar.loadCache(data_);   // instant; data_.valid may be false
    render(ctx);                     // show cached immediately
    if (configured_) {
        // Freshness gate — same interval as weather (no calendar-specific
        // schedule exists; see calendar spec §4). Manual tap still forces.
        uint32_t now  = static_cast<uint32_t>(ctx.rtc.nowUnix());
        uint16_t rmin = ctx.config.weatherRefreshMin();
        if (rmin == 0) rmin = 30;
        bool fresh = data_.valid && now >= data_.fetched_unix &&
                     (now - data_.fetched_unix) < static_cast<uint32_t>(rmin) * 60U;
        if (!fresh) refresh(ctx);
    }
}

void CalendarApp::leave(AppContext& ctx) {
    ctx.calendar.releaseWifi();
}

void CalendarApp::onInput(const InputEvent& e, AppContext& ctx) {
    if (e.kind != InputEvent::Kind::TouchUp) return;
    if (!configured_) {                        // подсказка → в настройки
        ctx.switcher.requestSwitch("settings");
        return;
    }
    if (millis() - last_refresh_ms_ < 10000) return;
    refresh(ctx);
}

void CalendarApp::refresh(AppContext& ctx) {
    offline_ = !ctx.calendar.ensureWifi();
    if (!offline_) {
        // static, not stack: sizeof(CalendarData) ~2.9 KB is reserved for this
        // frame from entry, and render() below drives the deep, stack-hungry
        // FreeType glyph chain — a stack local here risks the same loopTask
        // overflow as the screensaver's drawClockInto. Copied into data_ on
        // success, so a static holds no state we depend on across calls.
        static CalendarData cd;
        if (ctx.calendar.fetch(cd)) {
            data_ = cd;
            ctx.calendar.saveCache(cd);
        }
    }
    render(ctx);
    last_refresh_ms_ = millis();
}

void CalendarApp::render(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.setTextColor(15);
    c.drawString(tr(Str::app_calendar), kHeaderTextX, kHeaderTextY);

    // Текущая дата справа в заголовке.
    {
        char date[32];
        ctx.rtc.formatDate(date, sizeof(date));
        fonts.apply(c, FontFace::Serif, 22);
        c.drawString(date, kScreenW - 190, kHeaderTextY + 10);
    }
    if (offline_) {
        fonts.apply(c, FontFace::Serif, 18);
        c.drawString(tr(Str::weather_offline), kScreenW - 110, kHeaderTextY + 36);
    }

    if (!configured_) {
        fonts.apply(c, FontFace::Serif, 26);
        c.drawString(tr(Str::cal_not_configured), 40, kScreenH / 2 - 40);
        fonts.apply(c, FontFace::Serif, 22);
        c.drawString(tr(Str::cal_open_settings), 40, kScreenH / 2 + 8);
        ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
        return;
    }

    const uint32_t now = static_cast<uint32_t>(ctx.rtc.nowUnix());
    const uint32_t today0 = now - now % 86400u;

    int16_t y = kContentTopY;
    const int16_t yMax = kScreenH - 70;   // низ — под футер
    int shown = 0, hidden = 0;

    for (int day = 0; day < 7; ++day) {
        const uint32_t d0 = today0 + static_cast<uint32_t>(day) * 86400u;
        const uint32_t d1 = d0 + 86400u;

        // События, пересекающие день и не полностью прошедшие.
        bool day_has = false;
        for (uint8_t i = 0; i < data_.count; ++i) {
            const CalendarEvent& e = data_.events[i];
            if (e.start_local >= d1 || e.end_local <= d0) continue;   // не этот день
            if (day == 0 && e.end_local <= now) continue;             // уже прошло
            if (!e.all_day && day > 0 && e.start_local < d0) continue; // timed через полночь — только в дне начала

            if (y + 76 > yMax) { hidden++; continue; }   // не влезает — считаем

            if (!day_has) {   // заголовок дня
                char hdr[40];
                if (day == 0)      snprintf(hdr, sizeof(hdr), "%s", tr(Str::cal_today));
                else if (day == 1) snprintf(hdr, sizeof(hdr), "%s", tr(Str::cal_tomorrow));
                else {
                    time_t t = static_cast<time_t>(d0);
                    struct tm g; gmtime_r(&t, &g);
                    snprintf(hdr, sizeof(hdr), "%s, %d %s",
                             daysShort()[g.tm_wday], g.tm_mday, monthsShort()[g.tm_mon]);
                }
                fonts.apply(c, FontFace::MonoBold, 20);
                c.drawString(hdr, 30, y);
                c.drawLine(30, y + 28, kScreenW - 30, y + 28, 8);
                y += 38;
                day_has = true;
            }

            // Строка события: время (MonoBold, фикс. колонка) + текст (Serif).
            // All-day → "● весь день" (спека §6 mockup); буфер с запасом под
            // UTF-8 "весь день" (~17 Б) + маркер.
            char timecol[24];
            if (e.all_day) {
                snprintf(timecol, sizeof(timecol), "\xE2\x97\x8F %s", tr(Str::cal_all_day));   // ● весь день
            } else {
                char t1[8], t2[8];
                ctx.rtc.formatHHMMUnix(e.start_local, t1, sizeof(t1));
                ctx.rtc.formatHHMMUnix(e.end_local, t2, sizeof(t2));
                snprintf(timecol, sizeof(timecol), "%s\xE2\x80\x93%s", t1, t2);   // –
            }
            fonts.apply(c, FontFace::MonoBold, 22);
            c.drawString(timecol, 36, y + 4);

            char text[120];
            const char* title = e.title[0] ? e.title : tr(Str::cal_untitled);
            if (e.location[0]) {
                char comp[96];
                snprintf(comp, sizeof(comp), "%s \xC2\xB7 %s", title, e.location);   // ·
                utf8Clip(text, sizeof(text), comp, 24);
            } else {
                utf8Clip(text, sizeof(text), title, 24);
            }
            fonts.apply(c, FontFace::Serif, 24);
            c.drawString(text, 210, y + 2);
            y += 40;
            shown++;
        }
        if (day_has) y += 10;   // отбивка между днями
    }

    if (shown == 0 && hidden == 0) {
        fonts.apply(c, FontFace::Serif, 26);
        c.drawString(tr(Str::cal_no_events), kScreenW / 2 - 90, kScreenH / 2 - 20);
    }
    if (hidden > 0) {
        char more[24];
        snprintf(more, sizeof(more), tr(Str::cal_more_fmt), hidden);
        fonts.apply(c, FontFace::Serif, 20);
        c.drawString(more, 36, yMax + 6);
    }

    // Футер «Обновлено HH:MM».
    if (data_.fetched_unix != 0) {
        char hhmm[8]; ctx.rtc.formatHHMMUnix(data_.fetched_unix, hhmm, sizeof(hhmm));
        char upd[28]; snprintf(upd, sizeof(upd), tr(Str::weather_updated_fmt), hhmm);
        fonts.apply(c, FontFace::Serif, 18);
        c.drawString(upd, kScreenW - 190, kScreenH - 34);
    }

    ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
}

} // namespace paperos
