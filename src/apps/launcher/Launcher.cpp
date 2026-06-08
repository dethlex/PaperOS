#include "Launcher.h"
#include "i18n/Strings.h"
#include "hal/Display.h"
#include "hal/Sd.h"
#include "hal/Rtc.h"
#include "hal/Battery.h"
#include "framework/AppSwitcher.h"
#include "framework/LauncherOrder.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/IconKit.h"
#include <M5EPD.h>
#include <cstdio>

namespace paperos {

// Status-уголок в шапке: время + батарея. Все значения кратны 4
// (требование WritePartGram4bpp для partial-push). Не пересекается с заголовком
// (слева, x=30) и карточками (y>=80).
static constexpr int kStatusX = 284;
static constexpr int kStatusY = 0;
static constexpr int kStatusW = 256;
static constexpr int kStatusH = 72;
static_assert(kStatusX % 4 == 0 && kStatusY % 4 == 0 &&
              kStatusW % 4 == 0 && kStatusH % 4 == 0,
              "status rect must be 4-aligned for WritePartGram4bpp");

void Launcher::drawStatus(M5EPD_Canvas& target, AppContext& ctx, int ox, int oy) {
    Fonts fonts;
    target.setTextColor(15);

    char hhmm[8];
    ctx.rtc.formatHHMM(hhmm, sizeof(hhmm));
    fonts.apply(target, FontFace::Serif, 32);
    target.drawString(hhmm, (kStatusX + 4) - ox, (kStatusY + 16) - oy);

    BatteryReading bat = ctx.battery.read();
    // Иконка по центру строки часов: clock(32px)@+16 → центр +32; icon(26) → top +19.
    IconKit::battery(target, (kStatusX + 110) - ox, (kStatusY + 19) - oy, 56, 26, bat.percent);
    char bs[8]; snprintf(bs, sizeof(bs), "%u%%", bat.percent);
    fonts.apply(target, FontFace::Serif, 22);   // мельче (было 28) — "100%" больше не упирается в правый край
    target.drawString(bs, (kStatusX + 176) - ox, (kStatusY + 20) - oy);
}

void Launcher::enter(AppContext& ctx) {
    cards_.clear();
    scroll_offset_ = 0;                          // always open at the top
    const auto order = launcherAppOrder(ctx.switcher.registeredIds());

    constexpr int16_t margin = 30, gap = 20, top = kContentTopY;
    constexpr int16_t card_w = (kScreenW - margin * 2 - gap) / 2;
    constexpr int16_t card_h = 260;              // FIXED (no more adaptive shrink)

    int16_t x = margin, y = top;
    int col = 0;
    for (const auto& id : order) {
        const char* lbl =
            id == "reader"     ? tr(Str::app_reader) :
            id == "ha"         ? tr(Str::app_ha) :
            id == "weather"    ? tr(Str::app_weather) :
            id == "games"      ? tr(Str::app_games) :
            id == "settings"   ? tr(Str::app_settings) :
            id == "fileserver" ? tr(Str::app_files) : nullptr;
        std::string label = lbl ? std::string(lbl) : id;   // fallback: id as its own label
        cards_.push_back({id, label, {x, y, card_w, card_h}});
        if (++col == 2) { col = 0; x = margin; y += card_h + gap; }
        else            { x += card_w + gap; }
    }
    content_bottom_ = cards_.empty() ? top
                     : cards_.back().bounds.y + card_h;
    renderAll(ctx);
}

void Launcher::renderAll(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);                       // 0 = white (paper), 15 = black (ink)
    c.setTextColor(15);

    Fonts fonts;

    for (const auto& card : cards_) {
        const int16_t cy = card.bounds.y - scroll_offset_;          // screen-space top
        if (cy + card.bounds.h <= kContentTopY || cy >= kScreenH) continue;   // fully off-screen
        c.drawRect(card.bounds.x, cy, card.bounds.w, card.bounds.h, 15);

        const int s = 110;                                          // icon box
        const int ix = card.bounds.x + (card.bounds.w - s) / 2;
        const int iy = cy + 34;
        IconKit::app(c, card.app_id, ix, iy, s);

        // Label left-aligned at a fixed inset. Do NOT center via textWidth:
        // M5EPD smooth-font textWidth underestimates capital-Cyrillic advance
        // (CLAUDE.md §TTF), so centering drifts the label right and unevenly.
        // The codebase positions Cyrillic with fixed offsets, never textWidth.
        fonts.apply(c, FontFace::Serif, 32);
        c.drawString(card.label.c_str(), card.bounds.x + 20, cy + card.bounds.h - 60);
    }

    if (!ctx.sd.present()) {
        fonts.apply(c, FontFace::Serif, 22);
        c.drawString(tr(Str::launcher_sd_unavailable), 20, kScreenH - 36);
    }

    // Scroll indicator on the right edge — only when content overflows (mirror HA).
    {
        const int visible_h = kScreenH - kContentTopY;
        const int content_h = content_bottom_ - kContentTopY;
        if (content_h > visible_h) {
            const int track_x = kScreenW - 12, track_y = kContentTopY;
            c.drawRect(track_x, track_y, 8, visible_h, 12);
            int thumb_h = visible_h * visible_h / content_h;
            if (thumb_h < 24) thumb_h = 24;
            const int span = content_h - visible_h;
            const int thumb_y = track_y + (visible_h - thumb_h) *
                          (span > 0 ? scroll_offset_ : 0) / (span > 0 ? span : 1);
            c.fillRect(track_x, thumb_y, 8, thumb_h, 15);
        }
    }

    // Repaint the header band over any card content that scrolled up under it,
    // then draw the title on top so scrolled tiles tuck cleanly beneath the header.
    c.fillRect(0, 0, kScreenW, kContentTopY, 0);
    fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.drawString("PaperOS", kHeaderTextX, kHeaderTextY);

    drawStatus(c, ctx, 0, 0);
    last_minute_ = ctx.rtc.minute();
    ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
}

void Launcher::onInput(const InputEvent& e, AppContext& ctx) {
    if (e.kind == InputEvent::Kind::NavUp)   { scrollBy(-1, ctx); return; }
    if (e.kind == InputEvent::Kind::NavDown) { scrollBy(+1, ctx); return; }
    if (e.kind != InputEvent::Kind::TouchUp) return;

    // cards_ bounds are absolute; translate the touch into layout space.
    const Point p{e.x, static_cast<int16_t>(e.y + scroll_offset_)};
    for (const auto& card : cards_) {
        if (!card.bounds.contains(p)) continue;
        if (!ctx.sd.present() && (card.app_id == "reader" || card.app_id == "ha")) {
            auto& c = ctx.display.canvas();
            Fonts fonts; fonts.apply(c, FontFace::Serif, 28);
            c.setTextColor(15);
            c.fillRect(40, kScreenH - 120, kScreenW - 80, 70, 0);
            c.drawRect(40, kScreenH - 120, kScreenW - 80, 70, 12);
            c.drawString(tr(Str::launcher_need_sd), 70, kScreenH - 102);
            ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Quick);
            return;
        }
        ctx.switcher.requestSwitch(card.app_id);
        return;
    }
}

void Launcher::scrollBy(int dir, AppContext& ctx) {
    const int visible_h = kScreenH - kContentTopY;          // content viewport (header fixed above)
    const int content_h = content_bottom_ - kContentTopY;
    const int max_off = (content_h > visible_h) ? (content_h - visible_h) : 0;
    const int step = (visible_h - 280) * dir;      // 280 = card_h(260) + gap(20): one tile row, keeps a row of context
    int next = scroll_offset_ + step;
    if (next < 0) next = 0;
    if (next > max_off) next = max_off;
    if (next == scroll_offset_) return;            // already at the edge
    scroll_offset_ = next;
    renderAll(ctx);
}

void Launcher::tick(AppContext& ctx) {
    int m = ctx.rtc.minute();
    if (m == last_minute_) return;     // меняем уголок только на границе минуты
    last_minute_ = m;

    // Выделенный sub-canvas только под status-уголок (живёт в PSRAM, аллоцируется
    // один раз). Фон под уголком — чистый белый, кэш не нужен: заливаем 0 и рисуем.
    // Display форсит FULL раз в N partial'ов, но только в пределах этого rect'а —
    // ghosting карточек снимает лишь следующий renderAll (полный GC16); на практике
    // это ограничено idle-таймаутом (лаунчер уходит в скринсейвер раньше).
    static M5EPD_Canvas status_canvas(&M5.EPD);
    static bool ready = false;
    if (!ready) { status_canvas.createCanvas(kStatusW, kStatusH); ready = true; }

    status_canvas.fillCanvas(0);
    drawStatus(status_canvas, ctx, kStatusX, kStatusY);
    ctx.display.pushSubCanvas(status_canvas, kStatusX, kStatusY, PushMode::Partial);
}

} // namespace paperos
