#pragma once
#include <cstdint>
#include <vector>

namespace paperos {

// Pure 8x8 minesweeper. Mines injected explicitly (positions excluding the
// first-tapped cell) so the device wrapper can guarantee a safe first reveal
// and tests can be deterministic. No M5 deps (native-tested).
class MinesweeperCore {
public:
    enum class State : uint8_t { Playing, Won, Lost };
    static constexpr int W = 8;
    static constexpr int H = 8;
    static constexpr int CELLS = 64;
    static constexpr int MINES = 10;

    void  reset();                                                  // all covered, no mines
    void  placeMines(int firstCell, const std::vector<int>& positions); // skips firstCell
    bool  minesPlaced() const { return placed_; }
    void  reveal(int x, int y);                                     // flood; no-op if flagged/revealed/over
    void  toggleFlag(int x, int y);
    bool  isMine(int x, int y) const;
    bool  isRevealed(int x, int y) const;
    bool  isFlagged(int x, int y) const;
    int   adjacent(int x, int y) const;                            // mine count around (valid after placeMines)
    State state() const { return state_; }
    int   minesRemaining() const;                                  // MINES - flags (may go negative)

private:
    bool    mine_[CELLS]     = {};
    bool    revealed_[CELLS] = {};
    bool    flagged_[CELLS]  = {};
    uint8_t adj_[CELLS]      = {};
    bool    placed_ = false;
    State   state_  = State::Playing;

    static bool inBounds(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }
    static int  idx(int x, int y) { return y * W + x; }
    void computeAdjacency();
    void floodReveal(int x, int y);
    bool allSafeRevealed() const;
};

} // namespace paperos
