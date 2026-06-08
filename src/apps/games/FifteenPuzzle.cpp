#include "apps/games/FifteenPuzzle.h"
#include "apps/games/GamesUi.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/Geometry.h"
#include <M5EPD.h>
#include <esp_system.h>     // esp_random()
#include <cstdio>

namespace paperos {

static constexpr int kCell = 120;
static constexpr int kX = 30;
static constexpr int kY = 130;
static constexpr int kWH = kCell * FifteenCore::N;   // 480
static constexpr Rect kNewBtn{155, 680, 230, 100};

void FifteenPuzzle::reset() {
    core_.reset();
    for (int tries = 0; tries < 8; ++tries) {
        for (int i = 0; i < 200; ++i) {
            auto n = core_.holeNeighbors();
            int pick = n[esp_random() % n.size()];
            core_.slide(pick);
        }
        if (!core_.isSolved()) break;          // don't start already solved
    }
    core_.resetMoveCount();
    solved_   = false;
    start_ms_ = millis();
    full_     = true;
}

void FifteenPuzzle::onInput(const InputEvent& e) {
    if (e.kind != InputEvent::Kind::TouchUp) return;
    Point p{e.x, e.y};
    if (kNewBtn.contains(p)) { reset(); return; }
    if (solved_) return;

    Rect board{kX, kY, kWH, kWH};
    if (!board.contains(p)) return;
    int col = (e.x - kX) / kCell;
    int row = (e.y - kY) / kCell;
    if (col < 0 || col >= FifteenCore::N || row < 0 || row >= FifteenCore::N) return;
    if (core_.slide(row * FifteenCore::N + col) && core_.isSolved()) {
        solved_   = true;
        solve_ms_ = millis();
    }
}

void FifteenPuzzle::render(M5EPD_Canvas& c) {
    Fonts fonts;
    c.setTextColor(15);

    char status[64];
    if (solved_) {
        uint32_t sec = (solve_ms_ - start_ms_) / 1000;
        snprintf(status, sizeof(status), tr(Str::fifteen_solved_fmt),
                 core_.moves(), (unsigned)(sec / 60), (unsigned)(sec % 60));
    } else {
        snprintf(status, sizeof(status), tr(Str::fifteen_moves_fmt), core_.moves());
    }
    fonts.apply(c, FontFace::Serif, 28);
    c.drawString(status, 30, 86);

    // One unified board square + 4x4 grid lines (not 16 separate tiles).
    c.drawRect(kX, kY, kWH, kWH, 15);
    for (int i = 1; i < FifteenCore::N; ++i) {
        c.drawLine(kX + i * kCell, kY, kX + i * kCell, kY + kWH, 15);
        c.drawLine(kX, kY + i * kCell, kX + kWH, kY + i * kCell, 15);
    }
    for (int cell = 0; cell < FifteenCore::COUNT; ++cell) {
        int v = core_.at(cell);
        if (v <= 0) continue;                  // hole — leave the cell empty
        int x = kX + (cell % FifteenCore::N) * kCell;
        int y = kY + (cell / FifteenCore::N) * kCell;
        char num[3]; snprintf(num, sizeof(num), "%d", v);
        // Center in the FULL cell so 1- and 2-digit numbers both sit centered.
        gamesui::centerAscii(c, {static_cast<int16_t>(x), static_cast<int16_t>(y),
                                 kCell, kCell}, num, 48);
    }
    gamesui::button(c, kNewBtn, tr(Str::common_new_game), false);
}

} // namespace paperos
