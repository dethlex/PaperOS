#include "apps/games/core/MinesweeperCore.h"

namespace paperos {

void MinesweeperCore::reset() {
    for (int i = 0; i < CELLS; ++i) {
        mine_[i] = revealed_[i] = flagged_[i] = false;
        adj_[i] = 0;
    }
    placed_ = false;
    state_  = State::Playing;
}

// Precondition: called once per game, after reset(), before any reveal() (the
// device wrapper guards on minesPlaced()). Intentionally clears only mine_ and
// recomputes adjacency — it does NOT touch flagged_/revealed_/state_, so flags
// the player placed on covered cells before the first reveal are preserved.
void MinesweeperCore::placeMines(int firstCell, const std::vector<int>& positions) {
    for (int i = 0; i < CELLS; ++i) mine_[i] = false;
    for (int p : positions) {
        if (p >= 0 && p < CELLS && p != firstCell) mine_[p] = true;
    }
    placed_ = true;
    computeAdjacency();
}

void MinesweeperCore::computeAdjacency() {
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int n = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (inBounds(nx, ny) && mine_[idx(nx, ny)]) ++n;
                }
            adj_[idx(x, y)] = static_cast<uint8_t>(n);
        }
    }
}

bool MinesweeperCore::isMine(int x, int y) const     { return inBounds(x, y) && mine_[idx(x, y)]; }
bool MinesweeperCore::isRevealed(int x, int y) const { return inBounds(x, y) && revealed_[idx(x, y)]; }
bool MinesweeperCore::isFlagged(int x, int y) const  { return inBounds(x, y) && flagged_[idx(x, y)]; }
int  MinesweeperCore::adjacent(int x, int y) const   { return inBounds(x, y) ? adj_[idx(x, y)] : 0; }

int MinesweeperCore::minesRemaining() const {
    int flags = 0;
    for (int i = 0; i < CELLS; ++i) if (flagged_[i]) ++flags;
    return MINES - flags;
}

void MinesweeperCore::toggleFlag(int x, int y) {
    if (state_ != State::Playing || !inBounds(x, y)) return;
    int i = idx(x, y);
    if (revealed_[i]) return;
    flagged_[i] = !flagged_[i];
}

void MinesweeperCore::reveal(int x, int y) {
    if (state_ != State::Playing || !inBounds(x, y)) return;
    int i = idx(x, y);
    if (flagged_[i] || revealed_[i]) return;
    if (mine_[i]) { revealed_[i] = true; state_ = State::Lost; return; }
    floodReveal(x, y);
    if (allSafeRevealed()) state_ = State::Won;
}

void MinesweeperCore::floodReveal(int x, int y) {
    if (!inBounds(x, y)) return;
    int i = idx(x, y);
    if (revealed_[i] || flagged_[i] || mine_[i]) return;
    revealed_[i] = true;
    if (adj_[i] != 0) return;                       // border number — stop expanding
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            floodReveal(x + dx, y + dy);
        }
}

bool MinesweeperCore::allSafeRevealed() const {
    for (int i = 0; i < CELLS; ++i)
        if (!mine_[i] && !revealed_[i]) return false;
    return true;
}

} // namespace paperos
