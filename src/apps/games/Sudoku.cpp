#include "apps/games/Sudoku.h"
#include "apps/games/GamesUi.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/Geometry.h"
#include <M5EPD.h>
#include <esp_system.h>     // esp_random()

namespace paperos {

static constexpr int kCell = 54;
static constexpr int kX = 27;
static constexpr int kY = 120;
static constexpr int kWH = kCell * SudokuCore::N;   // 486
static constexpr Rect kEraseBtn{30,  720, 230, 90};
static constexpr Rect kNewBtn  {280, 720, 230, 90};

// Digit-pad button i (0..8), representing value i+1. One formula, used by both
// onInput hit-test and render draw.
static Rect digitRect(int i) {
    return Rect{static_cast<int16_t>(i * 60), 620, 56, 80};
}

void Sudoku::reset() {
    core_.load(SudokuCore::puzzle(static_cast<int>(esp_random() % SudokuCore::puzzleCount())));
    sel_r_ = sel_c_ = -1;
    full_ = true;
}

void Sudoku::onInput(const InputEvent& e) {
    if (e.kind != InputEvent::Kind::TouchUp) return;
    Point p{e.x, e.y};
    if (kNewBtn.contains(p)) { reset(); return; }
    if (kEraseBtn.contains(p)) {
        if (sel_r_ >= 0) core_.clear(sel_r_, sel_c_);
        return;
    }
    for (int i = 0; i < 9; ++i) {                    // digit pad
        if (digitRect(i).contains(p)) {
            if (sel_r_ >= 0) core_.set(sel_r_, sel_c_, i + 1);
            return;
        }
    }
    Rect board{kX, kY, kWH, kWH};                    // select a cell
    if (board.contains(p)) {
        sel_c_ = (e.x - kX) / kCell;
        sel_r_ = (e.y - kY) / kCell;
    }
}

void Sudoku::render(M5EPD_Canvas& c) {
    Fonts fonts;
    c.setTextColor(15);
    // Header already shows the game title — only surface the win here.
    if (core_.isSolved()) {
        fonts.apply(c, FontFace::Serif, 28);
        c.drawString(tr(Str::sudoku_solved), 30, 86);
    }

    for (int r = 0; r < SudokuCore::N; ++r) {
        for (int cc = 0; cc < SudokuCore::N; ++cc) {
            int x = kX + cc * kCell, y = kY + r * kCell;
            if (r == sel_r_ && cc == sel_c_) c.fillRect(x, y, kCell, kCell, 6);   // selection
            c.drawRect(x, y, kCell, kCell, 15);
            int v = core_.at(r, cc);
            if (v > 0) {
                char d[2] = { static_cast<char>('0' + v), 0 };
                fonts.apply(c, core_.isGiven(r, cc) ? FontFace::MonoBold : FontFace::Serif, 40);
                c.setTextColor(15);
                int w = c.textWidth(d);
                c.drawString(d, x + (kCell - w) / 2, y + (kCell - 40) / 2);
                if (core_.hasConflict(r, cc))
                    c.drawRect(x + 3, y + 3, kCell - 6, kCell - 6, 15);          // conflict marker
            }
        }
    }
    // Thick 3x3 block separators — 3 px (vs the 1 px cell grid) so the nine
    // sub-squares read clearly. Drawn AFTER the cells so they overwrite the
    // thin borders at the block boundaries. fillRect centred on the grid line
    // (xx-1..xx+1) is cleaner than stacking drawLine calls.
    for (int k = 0; k <= 3; ++k) {
        int xx = kX + k * 3 * kCell;
        int yy = kY + k * 3 * kCell;
        c.fillRect(xx - 1, kY, 3, kWH + 1, 15);   // vertical block separator
        c.fillRect(kX, yy - 1, kWH + 1, 3, 15);   // horizontal block separator
    }
    for (int i = 0; i < 9; ++i) {                    // digit pad
        Rect b = digitRect(i);
        c.drawRect(b.x, b.y, b.w, b.h, 15);
        char d[2] = { static_cast<char>('1' + i), 0 };
        gamesui::centerAscii(c, b, d, 40);
    }
    gamesui::button(c, kEraseBtn, tr(Str::sudoku_erase), false);
    gamesui::button(c, kNewBtn, tr(Str::common_new_game), false);
}

} // namespace paperos
