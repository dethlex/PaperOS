#include "apps/games/TicTacToe.h"
#include "apps/games/GamesUi.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/Geometry.h"
#include <M5EPD.h>

namespace paperos {

using Cell   = TicTacToeCore::Cell;
using Result = TicTacToeCore::Result;

static constexpr int  kCell      = 160;
static constexpr int  kBoardX    = 30;
static constexpr int  kBoardY    = 120;
static constexpr int  kBoardWH   = kCell * 3;        // 480
static constexpr Rect kNewBtn    {30, 700, 230, 100};
static constexpr Rect kModeBtn   {280, 700, 230, 100};
static constexpr int  kMarkArm   = 48;               // X arm half-length
static constexpr int  kMarkRadius = 52;              // O outer radius

void TicTacToe::reset() {
    core_.reset();
    turn_ = Cell::X;
    full_ = true;
}

void TicTacToe::onInput(const InputEvent& e) {
    if (e.kind != InputEvent::Kind::TouchUp) return;
    Point p{e.x, e.y};
    if (kNewBtn.contains(p))  { reset(); return; }
    if (kModeBtn.contains(p)) { ai_ = !ai_; reset(); return; }
    if (core_.gameOver()) return;

    Rect board{kBoardX, kBoardY, kBoardWH, kBoardWH};
    if (!board.contains(p)) return;
    int col = (e.x - kBoardX) / kCell;
    int row = (e.y - kBoardY) / kCell;
    if (col < 0 || col > 2 || row < 0 || row > 2) return;
    int idx = row * 3 + col;

    if (ai_) {
        if (core_.place(idx, Cell::X) && !core_.gameOver()) {
            int m = core_.bestMove(Cell::O);
            if (m >= 0) core_.place(m, Cell::O);
        }
    } else {
        if (core_.place(idx, turn_))
            turn_ = (turn_ == Cell::X) ? Cell::O : Cell::X;
    }
}

void TicTacToe::render(M5EPD_Canvas& c) {
    Fonts fonts;
    c.setTextColor(15);

    Result r = core_.result();
    const char* st;
    if      (r == Result::X)    st = ai_ ? tr(Str::ttt_you_won) : tr(Str::ttt_x_won);
    else if (r == Result::O)    st = ai_ ? tr(Str::ttt_ai_won)  : tr(Str::ttt_o_won);
    else if (r == Result::Draw) st = tr(Str::ttt_draw);
    else if (ai_)               st = tr(Str::ttt_your_move);
    else                        st = (turn_ == Cell::X) ? tr(Str::ttt_move_x) : tr(Str::ttt_move_o);
    fonts.apply(c, FontFace::Serif, 28);
    c.drawString(st, 30, 86);

    c.drawRect(kBoardX, kBoardY, kBoardWH, kBoardWH, 15);
    for (int i = 1; i < 3; ++i) {
        c.drawLine(kBoardX + i * kCell, kBoardY, kBoardX + i * kCell, kBoardY + kBoardWH, 15);
        c.drawLine(kBoardX, kBoardY + i * kCell, kBoardX + kBoardWH, kBoardY + i * kCell, 15);
    }
    for (int idx = 0; idx < 9; ++idx) {
        Cell m = core_.at(idx);
        if (m == Cell::Empty) continue;
        int cx = kBoardX + (idx % 3) * kCell + kCell / 2;
        int cy = kBoardY + (idx / 3) * kCell + kCell / 2;
        if (m == Cell::X) {
            c.drawLine(cx - kMarkArm, cy - kMarkArm, cx + kMarkArm, cy + kMarkArm, 15);
            c.drawLine(cx + kMarkArm, cy - kMarkArm, cx - kMarkArm, cy + kMarkArm, 15);
            c.drawLine(cx - kMarkArm, cy - kMarkArm + 1, cx + kMarkArm, cy + kMarkArm + 1, 15);   // 2px stroke
            c.drawLine(cx + kMarkArm, cy - kMarkArm + 1, cx - kMarkArm, cy + kMarkArm + 1, 15);
        } else {
            c.drawCircle(cx, cy, kMarkRadius,     15);
            c.drawCircle(cx, cy, kMarkRadius - 1, 15);
        }
    }
    gamesui::button(c, kNewBtn,  tr(Str::common_new_game), false);
    gamesui::button(c, kModeBtn, ai_ ? tr(Str::ttt_vs_ai) : tr(Str::ttt_vs_human), false);
}

} // namespace paperos
