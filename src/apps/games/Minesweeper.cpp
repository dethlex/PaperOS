#include "apps/games/Minesweeper.h"
#include "apps/games/GamesUi.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/Geometry.h"
#include <M5EPD.h>
#include <esp_system.h>     // esp_random()
#include <vector>
#include <cstdio>

namespace paperos {

using State = MinesweeperCore::State;

static constexpr int  kCell = 60;
static constexpr int  kX    = 30;
static constexpr int  kY    = 130;
static constexpr int  kWH   = kCell * MinesweeperCore::W;   // 480
static constexpr Rect kNewBtn {30,  680, 230, 100};
static constexpr Rect kModeBtn{280, 680, 230, 100};

void Minesweeper::reset() {
    core_.reset();
    flag_mode_ = false;
    full_ = true;
}

void Minesweeper::onInput(const InputEvent& e) {
    if (e.kind != InputEvent::Kind::TouchUp) return;
    Point p{e.x, e.y};
    if (kNewBtn.contains(p))  { reset(); return; }
    if (kModeBtn.contains(p)) { flag_mode_ = !flag_mode_; return; }
    if (core_.state() != State::Playing) return;

    Rect board{kX, kY, kWH, kWH};
    if (!board.contains(p)) return;
    int cx = (e.x - kX) / kCell;
    int cy = (e.y - kY) / kCell;
    if (cx < 0 || cx >= MinesweeperCore::W || cy < 0 || cy >= MinesweeperCore::H) return;

    if (flag_mode_) { core_.toggleFlag(cx, cy); return; }

    if (!core_.minesPlaced()) {                          // safe first reveal
        int first = cy * MinesweeperCore::W + cx;
        std::vector<int> pos;
        while ((int)pos.size() < MinesweeperCore::MINES) {
            int q = (int)(esp_random() % MinesweeperCore::CELLS);
            if (q == first) continue;
            bool dup = false;
            for (int z : pos) if (z == q) { dup = true; break; }
            if (!dup) pos.push_back(q);
        }
        core_.placeMines(first, pos);
    }
    core_.reveal(cx, cy);
}

void Minesweeper::render(M5EPD_Canvas& c) {
    Fonts fonts;
    c.setTextColor(15);

    State s = core_.state();
    char status[48];
    if      (s == State::Won)  snprintf(status, sizeof(status), "%s", tr(Str::mines_win));
    else if (s == State::Lost) snprintf(status, sizeof(status), "%s", tr(Str::mines_boom));
    else snprintf(status, sizeof(status), tr(Str::mines_status_fmt),
                  core_.minesRemaining(), flag_mode_ ? tr(Str::mines_flag) : tr(Str::mines_open));
    fonts.apply(c, FontFace::Serif, 28);
    c.drawString(status, 30, 86);

    for (int y = 0; y < MinesweeperCore::H; ++y) {
        for (int x = 0; x < MinesweeperCore::W; ++x) {
            int rx = kX + x * kCell, ry = kY + y * kCell;
            if (s == State::Lost && core_.isMine(x, y)) {
                c.fillRect(rx, ry, kCell, kCell, 0);
                c.drawRect(rx, ry, kCell, kCell, 15);
                c.fillCircle(rx + kCell / 2, ry + kCell / 2, 16, 15);
            } else if (core_.isRevealed(x, y)) {
                c.fillRect(rx, ry, kCell, kCell, 0);
                c.drawRect(rx, ry, kCell, kCell, 15);
                int a = core_.adjacent(x, y);
                if (a > 0) {
                    char d[2] = { static_cast<char>('0' + a), 0 };
                    gamesui::centerAscii(c, {static_cast<int16_t>(rx), static_cast<int16_t>(ry), kCell, kCell}, d, 36);
                }
            } else {
                c.fillRect(rx, ry, kCell, kCell, 8);          // covered = mid-gray
                c.drawRect(rx, ry, kCell, kCell, 15);
                if (core_.isFlagged(x, y)) {
                    int fx = rx + kCell / 2, top = ry + 12;
                    c.drawLine(fx, top, fx, ry + kCell - 12, 15);
                    c.fillTriangle(fx, top, fx + 18, top + 8, fx, top + 16, 15);
                }
            }
        }
    }
    gamesui::button(c, kNewBtn, tr(Str::common_new_game), false);
    gamesui::button(c, kModeBtn, flag_mode_ ? tr(Str::mines_flag) : tr(Str::mines_open), flag_mode_);
}

} // namespace paperos
