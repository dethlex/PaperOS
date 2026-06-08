#include "apps/games/GamesApp.h"
#include "hal/Display.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/IconKit.h"
#include <M5EPD.h>

namespace paperos {

Rect GamesApp::rowRect(int i) const {
    return Rect{30, static_cast<int16_t>(kContentTopY + i * kRowH), 480, 110};
}

void GamesApp::enter(AppContext& ctx) {
    mode_ = Mode::Menu;
    current_ = nullptr;
    renderMenu(ctx);
}

void GamesApp::onInput(const InputEvent& e, AppContext& ctx) {
    if (mode_ == Mode::Menu) {
        if (e.kind != InputEvent::Kind::TouchUp) return;
        Point p{e.x, e.y};
        for (int i = 0; i < kGameCount; ++i) {
            if (rowRect(i).contains(p)) {
                current_ = games_[i];
                current_->reset();
                mode_ = Mode::InGame;
                renderGame(ctx, true);
                return;
            }
        }
        return;
    }
    // InGame — only TouchUp drives the game. The four games are touch-only;
    // forwarding Nav would re-render + push for no state change (e-ink churn).
    // G38/back is handled by onBack(), not here.
    if (e.kind != InputEvent::Kind::TouchUp) return;
    if (!current_) return;
    current_->onInput(e);
    renderGame(ctx, current_->consumeFullRefresh());
}

bool GamesApp::onBack(AppContext& ctx) {
    if (mode_ == Mode::InGame) {
        mode_ = Mode::Menu;
        current_ = nullptr;
        renderMenu(ctx);
        return true;                     // consumed: went up one level
    }
    return false;                        // at top level -> Launcher
}

void GamesApp::renderMenu(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    c.setTextColor(15);
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.drawString(tr(Str::app_games), kHeaderTextX, kHeaderTextY);

    for (int i = 0; i < kGameCount; ++i) {
        Rect r = rowRect(i);
        c.drawRect(r.x, r.y, r.w, r.h, 15);
        IconKit::app(c, games_[i]->iconId(), r.x + 16, r.y + (r.h - 72) / 2, 72);
        fonts.apply(c, FontFace::Serif, 32);
        c.drawString(games_[i]->title(), r.x + 110, r.y + (r.h - 32) / 2);
    }
    ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
}

void GamesApp::renderGame(AppContext& ctx, bool full) {
    if (!current_) return;
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    c.setTextColor(15);
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.drawString(current_->title(), kHeaderTextX, kHeaderTextY);
    current_->render(c);
    ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, full ? PushMode::Full : PushMode::Partial);
}

} // namespace paperos
