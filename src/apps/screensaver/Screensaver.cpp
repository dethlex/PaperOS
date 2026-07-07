#include "Screensaver.h"
#include <Arduino.h>  // setCpuFrequencyMhz
#include "hal/Display.h"
#include "hal/Sd.h"
#include "hal/Rtc.h"
#include "services/Power.h"
#include "services/ConfigStore.h"
#include "framework/AppSwitcher.h"
#include "framework/ui/Fonts.h"
#include "services/WeatherService.h"
#include "services/WeatherData.h"
#include "services/CalendarService.h"
#include "services/calendar/CalendarSelect.h"
#include "i18n/Strings.h"
#include "framework/ui/IconKit.h"
#include "util/WmoCode.h"
#include "hal/Sht30.h"
#include "hal/Battery.h"
#include "util/Logger.h"
#include "framework/ui/PngDraw.h"
#include "util/Utf8.h"
#include "apps/screensaver/RotationPolicy.h"
#include <SD.h>
#include <TJpg_Decoder.h>
#include <PNGdec.h>
#include <vector>

extern const uint8_t kDefaultBgStart[] asm("_binary_data_default_wallpaper_bin_start");
extern const uint8_t kDefaultBgEnd[]   asm("_binary_data_default_wallpaper_bin_end");

namespace paperos {

static constexpr size_t kBgBytes = kScreenW * kScreenH / 2;   // 4bpp = 259200 bytes

// Decoded-wallpaper cache: one full 4bpp frame of the CURRENT photo in PSRAM.
// Process-static → survives app re-instantiation and light sleep; cold boot
// starts empty. Re-entering the lock screen with an unchanged photo becomes a
// memcpy (tens of ms) instead of a JPEG/PNG re-decode (seconds of CPU).
static uint8_t* s_bg_cache = nullptr;           // ps_malloc'd on first use
static uint16_t s_bg_cache_index = 0xFFFF;      // photoIndex held by the cache

// Unix time of the last photo rotation. Process-static: survives light sleep
// and app re-instantiation; resets on cold boot (0 → rotate on first entry).
static uint32_t s_last_rot_unix = 0;

// Region redrawn on every minute tick — encloses HH:MM (96px MonoBold near
// y≈720) and the date strip (28px Serif near y≈840) with margin. All four
// values must be multiples of 4 (IT8951 WritePartGram4bpp requirement —
// 4bpp packing means x and width are byte/word aligned in panel memory).
static constexpr int kClockRectX = 80;
static constexpr int kClockRectY = 724;   // sits lower on the screen (was 700)
static constexpr int kClockRectW = 380;
static constexpr int kClockRectH = 200;   // shorter; contents centred inside
static_assert(kClockRectX % 4 == 0 && kClockRectY % 4 == 0 &&
              kClockRectW % 4 == 0 && kClockRectH % 4 == 0,
              "clock rect must be 4-aligned for WritePartGram4bpp");

// Combined weather panel (outdoor + indoor rows) — partial-updated every minute
// (like the clock). All four values must be multiples of 4 (WritePartGram4bpp
// 4bpp alignment).
static constexpr int kPanelX = 16;
static constexpr int kPanelY = 20;
static constexpr int kPanelW = 508;
static constexpr int kPanelH = 132;
static_assert(kPanelX % 4 == 0 && kPanelY % 4 == 0 &&
              kPanelW % 4 == 0 && kPanelH % 4 == 0,
              "weather panel rect must be 4-aligned for WritePartGram4bpp");

// Picks the (photo_index)-th wallpaper from /paperos/wallpapers/.
// Both .jpg and .png are accepted; the renderer dispatches by extension.
// Returns empty string if none found.
static std::string pickWallpaper(Sd& sd, uint16_t index) {
    auto all = sd.list("/paperos/wallpapers", ".jpg");
    auto pngs = sd.list("/paperos/wallpapers", ".png");
    all.insert(all.end(), pngs.begin(), pngs.end());
    if (all.empty()) return {};
    return all[index % all.size()];
}

static bool endsWithIgnoreCase(const std::string& s, const char* suffix) {
    size_t sl = s.size(), nl = strlen(suffix);
    if (sl < nl) return false;
    for (size_t i = 0; i < nl; ++i) {
        char a = s[sl - nl + i]; char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

// TJpgDec callback: receives 16x16 blocks RGB565. We convert to grayscale (4bpp)
// and write into the canvas via drawPixel. For MVP this is acceptable speed.
static M5EPD_Canvas* s_decode_canvas = nullptr;
static bool jpegOut(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!s_decode_canvas) return false;
    for (int16_t j = 0; j < (int16_t)h; ++j) for (int16_t i = 0; i < (int16_t)w; ++i) {
        uint16_t c = bitmap[j * w + i];
        uint8_t r = (c >> 11) & 0x1F;
        uint8_t g = (c >> 5) & 0x3F;
        uint8_t b = c & 0x1F;
        uint8_t gray = ((r << 3) + (g << 2) + (b << 3)) / 3;     // approx
        uint8_t four = gray >> 4;                                  // 0..15
        s_decode_canvas->drawPixel(x + i, y + j, four);
    }
    return true;
}

// PNG SD-file callbacks (PNGdec needs them as function pointers).
static File         s_png_file;
static PNG&         s_png = sharedPng();   // shared decoder instance (see PngDraw.h)
static void* pngOpenCb(const char* filename, int32_t* size) {
    s_png_file = SD.open(filename, FILE_READ);
    if (!s_png_file) return nullptr;
    *size = s_png_file.size();
    return &s_png_file;
}
static void pngCloseCb(void*) { if (s_png_file) s_png_file.close(); }
static int32_t pngReadCb(PNGFILE*, uint8_t* buffer, int32_t length) {
    return s_png_file ? s_png_file.read(buffer, length) : 0;
}
static int32_t pngSeekCb(PNGFILE*, int32_t position) {
    return s_png_file ? s_png_file.seek(position) : 0;
}
static int pngDrawCb(PNGDRAW* pDraw) {
    if (!s_decode_canvas) return 0;
    // Ask PNGdec for this line converted to RGB565 little-endian.
    uint16_t line[kScreenW];
    int w = pDraw->iWidth > kScreenW ? kScreenW : pDraw->iWidth;
    s_png.getLineAsRGB565(pDraw, line, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);
    int y = pDraw->y;
    if (y < 0 || y >= kScreenH) return 1;
    for (int x = 0; x < w; ++x) {
        uint16_t c = line[x];
        uint8_t r = (c >> 11) & 0x1F;
        uint8_t g = (c >> 5) & 0x3F;
        uint8_t b = c & 0x1F;
        uint8_t gray = ((r << 3) + (g << 2) + (b << 3)) / 3;
        s_decode_canvas->drawPixel(x, y, gray >> 4);
    }
    return 1;
}

// Decode the PNG at `path` into `canvas`. Returns true on success.
static bool decodePngToCanvas(const std::string& path, M5EPD_Canvas& canvas) {
    s_decode_canvas = &canvas;
    int rc = s_png.open(path.c_str(), pngOpenCb, pngCloseCb, pngReadCb, pngSeekCb, pngDrawCb);
    bool ok = false;
    if (rc == PNG_SUCCESS) {
        // pngDrawCb's stack line buffer is kScreenW wide, but getLineAsRGB565
        // writes iWidth pixels unclamped (and PNG_MAX_BUFFERED_PIXELS admits far
        // wider images) — reject oversize wallpapers BEFORE decode runs, same
        // pattern as framework/ui/PngDraw.
        if (s_png.getWidth() > kScreenW) {
            LOG_WARN("ss", "PNG too wide (%d > %d), skipping", s_png.getWidth(), kScreenW);
            s_png.close();
        } else {
            rc = s_png.decode(nullptr, 0);
            s_png.close();
            ok = (rc == PNG_SUCCESS);
            if (!ok) LOG_WARN("ss", "PNG decode rc=%d", rc);
        }
    } else {
        LOG_WARN("ss", "PNG open rc=%d", rc);
    }
    s_decode_canvas = nullptr;
    return ok;
}

// Draw one value row (temp + drop + humidity) at panel-local origin (rx, ry).
// Columns are identical for the outdoor and indoor rows → symmetric. Icons-only,
// no condition text. The marker icon (weather glyph / house) is drawn by the caller.
static void drawWeatherRow(M5EPD_Canvas& t, Fonts& fonts, int rx, int ry,
                           bool valid, int temp_c, uint8_t hum) {
    fonts.apply(t, FontFace::Serif, 36);
    t.setTextColor(15);
    char s[16];
    if (valid) snprintf(s, sizeof(s), "%+d\xC2\xB0", temp_c);
    else       snprintf(s, sizeof(s), "--");
    t.drawString(s, rx + 92, ry + 14);
    if (valid) {
        IconKit::drop(t, rx + 230, ry + 12, 40);
        char h[8]; snprintf(h, sizeof(h), "%u%%", hum);
        t.drawString(h, rx + 280, ry + 14);
    }
}

// Everything drawWeatherPanel renders, in comparable form. If two consecutive
// minutes produce equal snapshots, the panel's GC16 push is skipped — the
// TPS65185 waveform is the dominant per-minute energy cost and outdoor/indoor
// readings change far less often than once a minute.
struct PanelSnapshot {
    bool    ok_out = false;
    int16_t t_out  = 0;
    uint8_t h_out  = 0;
    uint8_t cat    = 0;     // WeatherCat of the outdoor icon
    bool    ok_in  = false;
    int16_t t_in   = 0;
    uint8_t h_in   = 0;
    uint8_t bat    = 0;     // battery percent (drawn in the panel)
    // Fields behind an ok_* flag are only meaningful when the flag is set;
    // keep any future fields zero-defaulted and gated the same way, or operator== breaks skip-push.
    bool operator==(const PanelSnapshot& o) const {
        return ok_out == o.ok_out && t_out == o.t_out && h_out == o.h_out &&
               cat == o.cat && ok_in == o.ok_in && t_in == o.t_in &&
               h_in == o.h_in && bat == o.bat;
    }
};

static PanelSnapshot s_panel_prev;
static bool          s_panel_prev_valid = false;

// Combined weather panel: ONE box, icons + values only (no condition text).
// Outdoor row (weather icon + temp + humidity) above the indoor row (house icon
// + temp + humidity), symmetric columns. Outdoor comes from the NVS cache; indoor
// is read live from the SHT30. (ox, oy) is the screen-space origin of `target`
// (0,0 for the main canvas; the rect origin for the per-minute sub-canvas). The
// white fill is opaque, so no wallpaper restore is needed under it.
static PanelSnapshot drawWeatherPanel(M5EPD_Canvas& target, AppContext& ctx, int ox, int oy) {
    WeatherData wd;
    bool wok = ctx.weather.loadCache(wd) && wd.valid;
    IndoorReading in = ctx.sht30.read(ctx.config.indoorTempOffset());

    const int px = kPanelX - ox, py = kPanelY - oy;
    target.fillRect(px, py, kPanelW, kPanelH, 0);   // white box (0 = paper)
    target.drawRect(px, py, kPanelW, kPanelH, 15);  // ink border

    Fonts fonts;
    // Outdoor row (top): weather icon + temp + humidity.
    if (wok) IconKit::weather(target, catFromWmo(wd.current_wmo), px + 12, py + 8, 56);
    drawWeatherRow(target, fonts, px, py + 8, wok, wd.current_c, wd.current_humidity);
    // Indoor row (bottom): house icon + temp + humidity.
    IconKit::house(target, px + 12, py + 72, 56);
    drawWeatherRow(target, fonts, px, py + 72, in.valid, in.temp_c, in.humidity);

    // Battery — the free right-hand zone of the panel (the icons+values layout
    // leaves it empty). Refreshed by the same per-minute partial-push as the panel.
    BatteryReading bat = ctx.battery.read();
    const int bw = 72, bh = 30;
    const int bx = px + kPanelW - bw - 12;  // mirror the left edge: 12px from the border (like the left icon)
    IconKit::battery(target, bx, py + 34, bw, bh, bat.percent);
    fonts.apply(target, FontFace::Serif, 22);
    target.setTextColor(15);
    char bs[8]; snprintf(bs, sizeof(bs), "%u%%", bat.percent);
    target.drawString(bs, bx + 8, py + 78);

    PanelSnapshot snap;
    snap.ok_out = wok;
    if (wok) { snap.t_out = static_cast<int16_t>(wd.current_c);
               snap.h_out = wd.current_humidity;
               snap.cat   = static_cast<uint8_t>(catFromWmo(wd.current_wmo)); }
    snap.ok_in = in.valid;
    if (in.valid) { snap.t_in = in.temp_c; snap.h_in = in.humidity; }
    snap.bat = bat.percent;
    return snap;
}

void Screensaver::renderFull(AppContext& ctx, bool rotate_photo) {
    // photo_index lives in NVS now (RTC slow memory is wiped by the full
    // power-off sleep). Read it, advance on rotation, persist the new value.
    uint16_t photo = ctx.config.photoIndex();
    if (rotate_photo) { photo++; ctx.config.setPhotoIndex(photo); }

    auto& canvas = ctx.display.canvas();

    if (s_bg_cache && s_bg_cache_index == photo) {
        // Cache hit: restore the decoded wallpaper without touching the SD.
        memcpy(canvas.frameBuffer(), s_bg_cache, kBgBytes);
    } else {
        canvas.fillCanvas(0);

        std::string path = pickWallpaper(ctx.sd, photo);
        bool need_default = path.empty();
        if (!need_default) {
            if (endsWithIgnoreCase(path, ".png")) {
                if (!decodePngToCanvas(path, canvas)) need_default = true;
            } else {
                s_decode_canvas = &canvas;
                TJpgDec.setJpgScale(1);
                TJpgDec.setCallback(jpegOut);
                JRESULT jr = TJpgDec.drawSdJpg(0, 0, path.c_str());
                if (jr != JDR_OK) {
                    LOG_WARN("ss", "TJpgDec rc=%d on %s", static_cast<int>(jr), path.c_str());
                    need_default = true;
                }
                s_decode_canvas = nullptr;
            }
        }
        if (need_default) {
            // Default fallback wallpaper from INCBIN: raw 4bpp 540x960.
            const uint8_t* p = kDefaultBgStart;
            for (int16_t row = 0; row < kScreenH; ++row) {
                for (int16_t x = 0; x < kScreenW; x += 2) {
                    uint8_t byte = p[(row * kScreenW + x) / 2];
                    canvas.drawPixel(x,     row, (byte >> 4) & 0x0F);
                    canvas.drawPixel(x + 1, row, byte & 0x0F);
                }
            }
        }

        if (!s_bg_cache) s_bg_cache = static_cast<uint8_t*>(ps_malloc(kBgBytes));
        if (s_bg_cache) {
            // Snapshot the PURE wallpaper (before panel/clock are drawn on top).
            memcpy(s_bg_cache, canvas.frameBuffer(), kBgBytes);
            s_bg_cache_index = photo;
        }
        // ps_malloc failure → cache stays disabled, we just decode each time.
    }

    // Bake the combined weather panel (outdoor cache + live indoor) into the
    // post-refresh frame so it's present immediately after a full refresh;
    // renderMinuteTick re-pushes it live every minute. Record the snapshot so
    // the next minute tick doesn't force a redundant push of unchanged data.
    s_panel_prev = drawWeatherPanel(ctx.display.canvas(), ctx, 0, 0);
    s_panel_prev_valid = true;

    renderClockOverlay(ctx);
    ctx.display.pushFullClear();
}

// Draw HH:MM and date onto `target` canvas. (ox, oy) is the screen-space
// origin of `target` — i.e. if you render into the main full-screen canvas
// pass (0, 0); for the small clock canvas pass (kClockRectX, kClockRectY).
// This is what lets renderFull (main canvas) and renderMinuteTick
// (clock-only canvas) share the same text-placement logic.
static void drawClockInto(M5EPD_Canvas& target, AppContext& ctx, int ox, int oy) {
    // Box the clock+date in an opaque white rectangle with a border (same style as
    // the weather panel) — large digits over a photo read poorly. The box matches
    // the partial-push region (kClockRect), so it fully covers the previous content:
    // no need to restore the wallpaper under the clock from cache.
    const int bxp = kClockRectX - ox, byp = kClockRectY - oy;
    target.fillRect(bxp, byp, kClockRectW, kClockRectH, 0);   // 0 = white (paper)
    target.drawRect(bxp, byp, kClockRectW, kClockRectH, 15);  // 15 = black border

    // Center content relative to the box itself (MC_DATUM — font-metric based, more
    // reliable than textWidth for large glyphs), not via hardcoded screen offsets —
    // so the clock and date sit with equal margins inside the rectangle.
    const int cx = bxp + kClockRectW / 2;
    char hhmm[8];
    ctx.rtc.formatHHMM(hhmm, sizeof(hhmm));
    char date[32];
    ctx.rtc.formatDate(date, sizeof(date));

    Fonts fonts;
    target.setTextColor(15);
    target.setTextDatum(MC_DATUM);
    fonts.apply(target, FontFace::MonoBold, 96);
    target.drawString(hhmm, cx, byp + 72);    // часы — верхняя треть бокса
    fonts.apply(target, FontFace::Serif, 28);
    target.drawString(date, cx, byp + 140);   // дата — ниже центра

    // Ближайшее событие календаря (из NVS-кэша, сети тут нет — незыблемое
    // правило minute tick). Пусто/невалидно → строка не рисуется, бокс
    // остаётся белым (визуально ничего не меняется).
    {
        // NB: `static`, not a stack local. sizeof(CalendarData) is ~2.9 KB, and
        // the compiler reserves it for drawClockInto's whole frame from entry.
        // drawClockInto then drives the FreeType glyph-render chain (fonts.apply
        // → createRender → FT_Load_Glyph → …), which is ~20 frames deep and very
        // stack-hungry. cd on the frame overflowed the 16 KB loopTask stack →
        // Double exception. Screensaver rendering is single-threaded and
        // non-reentrant (like clock_canvas/panel_canvas below), so a static is
        // safe and keeps this big buffer off the stack.
        static CalendarData cd;
        if (ctx.calendar.loadCache(cd) && cd.valid) {
            NextEventPick pick = pickScreensaverEvent(
                cd, static_cast<uint32_t>(ctx.rtc.nowUnix()));
            if (pick.ev) {
                char t1[8], t2[8], line[96];
                if (pick.ongoing) {
                    ctx.rtc.formatHHMMUnix(pick.ev->start_local, t1, sizeof(t1));
                    ctx.rtc.formatHHMMUnix(pick.ev->end_local, t2, sizeof(t2));
                    snprintf(line, sizeof(line), "%s\xE2\x80\x93%s %s", t1, t2, pick.ev->title);
                } else if (pick.tomorrow && pick.ev->all_day) {
                    snprintf(line, sizeof(line), "%s \xE2\x97\x8F %s",
                             tr(Str::cal_tomorrow), pick.ev->title);
                } else if (pick.tomorrow) {
                    ctx.rtc.formatHHMMUnix(pick.ev->start_local, t1, sizeof(t1));
                    snprintf(line, sizeof(line), "%s %s %s",
                             tr(Str::cal_tomorrow), t1, pick.ev->title);
                } else {
                    ctx.rtc.formatHHMMUnix(pick.ev->start_local, t1, sizeof(t1));
                    snprintf(line, sizeof(line), "%s %s", t1, pick.ev->title);
                }
                // ~26 codepoints fit in 380px at Serif 22 (codepoint budget,
                // not textWidth — see CLAUDE.md TTF notes).
                char clip[96];
                utf8Clip(clip, sizeof(clip), line, 26);
                fonts.apply(target, FontFace::Serif, 22);
                target.drawString(clip, cx, byp + 176);
            }
        }
    }
    target.setTextDatum(TL_DATUM);                            // restore default
}

void Screensaver::renderClockOverlay(AppContext& ctx) {
    drawClockInto(ctx.display.canvas(), ctx, 0, 0);
}

void Screensaver::renderMinuteTick(AppContext& ctx) {
    // Small dedicated canvas covering only the clock+date region. Allocated
    // once (lives in PSRAM via TFT_eSprite) and reused on every tick.
    static M5EPD_Canvas clock_canvas(&M5.EPD);
    static bool clock_canvas_ready = false;
    if (!clock_canvas_ready) {
        clock_canvas.createCanvas(kClockRectW, kClockRectH);
        clock_canvas_ready = true;
    }

    // Draw clock with local coords (offset by the rect's screen-space origin).
    // drawClockInto fills the whole box opaquely (== the entire clock_canvas), so
    // neither a wallpaper restore from cache nor a pre-fill is needed.
    drawClockInto(clock_canvas, ctx, kClockRectX, kClockRectY);

    // Push only the clock rectangle (scoped to ~380×200, not the full 540×960).
    // FULL (GC16), not Partial (GL16): we now power-cut the IT8951 rail (GPIO23)
    // every sleep, so on wake its framebuffer is blank and a GL16 partial — which
    // transitions from the controller's last buffer — would ghost the previous
    // digits. GC16 drives the box to its target regardless of prior state. Only
    // this box waveform-refreshes; the wallpaper around it is retained by the
    // panel (no waveform there). Cost: a clean refresh of the box each tick.
    ctx.display.pushSubCanvas(clock_canvas, kClockRectX, kClockRectY, PushMode::Full);

    // Weather panel — re-read the cache + sensor and partial-push every minute
    // (outdoor + indoor live alongside the clock). Self-contained white box.
    static M5EPD_Canvas panel_canvas(&M5.EPD);
    static bool panel_canvas_ready = false;
    if (!panel_canvas_ready) {
        panel_canvas.createCanvas(kPanelW, kPanelH);
        panel_canvas_ready = true;
    }
    PanelSnapshot snap = drawWeatherPanel(panel_canvas, ctx, kPanelX, kPanelY);
    if (!s_panel_prev_valid || !(snap == s_panel_prev)) {
        ctx.display.pushSubCanvas(panel_canvas, kPanelX, kPanelY, PushMode::Full);  // GC16 — see clock note above
        s_panel_prev = snap;
        s_panel_prev_valid = true;
    }
}

void Screensaver::enter(AppContext& ctx) {
    // Photo rotation is driven strictly by photoRotateMin (Settings) — entering
    // the lock screen does NOT itself advance the photo (design 2026-07-06 §2).
    uint32_t now = static_cast<uint32_t>(ctx.rtc.nowUnix());
    bool rotate = rotationDue(now, s_last_rot_unix, ctx.config.photoRotateMin());
    if (rotate) s_last_rot_unix = now;
    renderFull(ctx, rotate);

    // The lock-screen loop runs at 80 MHz: SPI/I2C clocks derive from the
    // 80 MHz APB and are unaffected; with warm glyph caches the per-minute
    // render fits the window easily. 240 MHz is restored for WiFi windows
    // (TLS handshakes crawl at 80) and before returning to the interactive UI.
    setCpuFrequencyMhz(80);

    while (true) {
        ctx.power.lightSleep(60);

        if (ctx.power.lastWake() == WakeCause::Button) {
            setCpuFrequencyMhz(240);
            std::string id = ctx.switcher.idOfIndex(ctx.config.lastAppIndex());
            if (id.empty() || id == "screensaver") id = "launcher";
            ctx.switcher.requestSwitch(id);   // applied by main loop after enter() returns
            return;
        }

        // Timer tick. Weather/calendar refresh on the configured minute
        // boundary — data lands on screen via the panel's minute push below;
        // a full re-render (wallpaper decode + GC16) happens ONLY when the
        // photo itself rotates.
        int m = ctx.rtc.minute();
        uint16_t refresh = ctx.config.weatherRefreshMin();
        if (refresh == 0) refresh = 30;
        if ((m % refresh) == 0) {
            setCpuFrequencyMhz(240);
            ctx.weather.begin();
            if (ctx.weather.ensureWifi()) {
                WeatherData wd;
                if (ctx.weather.fetch(wd)) {
                    wd.fetched_unix = static_cast<uint32_t>(ctx.rtc.nowUnix());
                    ctx.weather.saveCache(wd);
                }
                // Calendar rides the same WiFi session (no schedule of its own,
                // calendar spec §4). Errors degrade silently to the cache.
                ctx.calendar.begin();
                if (ctx.calendar.configured()) {
                    // static: ~2.9 KB reserved on enter()'s frame, under which
                    // renderMinuteTick/renderFull below drive the deep FreeType
                    // chain — a stack local risks the same loopTask overflow.
                    static CalendarData cd;
                    if (ctx.calendar.fetch(cd)) ctx.calendar.saveCache(cd);
                }
            }
            ctx.weather.releaseWifi();
            setCpuFrequencyMhz(80);
        }

        // Re-read time after the (possibly multi-second) WiFi boundary so rotation
        // is judged on current time; do not merge with the earlier minute() read.
        uint32_t tick_now = static_cast<uint32_t>(ctx.rtc.nowUnix());
        if (rotationDue(tick_now, s_last_rot_unix, ctx.config.photoRotateMin())) {
            s_last_rot_unix = tick_now;
            renderFull(ctx, /*rotate_photo=*/true);
        } else {
            renderMinuteTick(ctx);
        }
    }
}

void Screensaver::onInput(const InputEvent&, AppContext& ctx) {
    // Reached only if invoked while still in Active loop (rare race).
    // Wake from sleep is handled in main.cpp.
    ctx.switcher.requestSwitch("launcher");
}

} // namespace paperos
