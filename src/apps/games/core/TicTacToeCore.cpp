#include "apps/games/core/TicTacToeCore.h"

namespace paperos {

using Cell   = TicTacToeCore::Cell;
using Result = TicTacToeCore::Result;

static const int kLines[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},  // rows
    {0,3,6},{1,4,7},{2,5,8},  // cols
    {0,4,8},{2,4,6}           // diagonals
};

void TicTacToeCore::reset() {
    for (int i = 0; i < 9; ++i) board_[i] = Cell::Empty;
}

Cell TicTacToeCore::at(int idx) const {
    return (idx >= 0 && idx < 9) ? board_[idx] : Cell::Empty;
}

bool TicTacToeCore::place(int idx, Cell mark) {
    if (idx < 0 || idx >= 9 || mark == Cell::Empty) return false;
    if (board_[idx] != Cell::Empty) return false;
    if (gameOver()) return false;
    board_[idx] = mark;
    return true;
}

Result TicTacToeCore::winnerOf(const Cell b[9]) {
    for (const auto& L : kLines) {
        Cell a = b[L[0]];
        if (a != Cell::Empty && a == b[L[1]] && a == b[L[2]])
            return (a == Cell::X) ? Result::X : Result::O;
    }
    for (int i = 0; i < 9; ++i) if (b[i] == Cell::Empty) return Result::None;
    return Result::Draw;
}

Result TicTacToeCore::result() const { return winnerOf(board_); }
bool    TicTacToeCore::gameOver() const { return result() != Result::None; }

static Cell opponent(Cell m) { return (m == Cell::X) ? Cell::O : Cell::X; }

// Mutates b in place for each recursive trial move and restores it afterward;
// callers must pass a mutable copy (bestMove already does this).
int TicTacToeCore::scoreFor(Cell b[9], Cell toMove, Cell me, int depth) {
    Result r = winnerOf(b);
    if (r == Result::Draw) return 0;
    if (r == Result::X) return (me == Cell::X) ? (10 - depth) : (depth - 10);
    if (r == Result::O) return (me == Cell::O) ? (10 - depth) : (depth - 10);

    int best = (toMove == me) ? -1000 : 1000;
    for (int i = 0; i < 9; ++i) {
        if (b[i] != Cell::Empty) continue;
        b[i] = toMove;
        int sc = scoreFor(b, opponent(toMove), me, depth + 1);
        b[i] = Cell::Empty;
        if (toMove == me) { if (sc > best) best = sc; }
        else              { if (sc < best) best = sc; }
    }
    return best;
}

int TicTacToeCore::bestMove(Cell mark) const {
    if (mark == Cell::Empty) return -1;
    Cell b[9];
    for (int i = 0; i < 9; ++i) b[i] = board_[i];
    if (winnerOf(b) != Result::None) return -1;

    int bestIdx = -1, bestScore = -1000;
    for (int i = 0; i < 9; ++i) {
        if (b[i] != Cell::Empty) continue;
        b[i] = mark;
        int sc = scoreFor(b, opponent(mark), mark, 1);
        b[i] = Cell::Empty;
        if (sc > bestScore) { bestScore = sc; bestIdx = i; }
    }
    return bestIdx;
}

} // namespace paperos
