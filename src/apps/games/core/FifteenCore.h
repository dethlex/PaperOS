#pragma once
#include <vector>

namespace paperos {

// Pure 4x4 sliding puzzle. board_[cell] holds the tile value (0 = hole).
// Scrambling is done by the wrapper applying random LEGAL slides (always
// solvable by construction); shuffle() replays an explicit move list and
// resets the move counter. No M5 deps (native-tested).
class FifteenCore {
public:
    static constexpr int N = 4;
    static constexpr int COUNT = 16;

    void reset();                                   // solved: 1..15, 0
    bool slide(int cell);                           // legal-only; true if a tile moved
    void shuffle(const std::vector<int>& moveCells);// apply each as slide, then zero moves
    void resetMoveCount() { moves_ = 0; }
    bool isSolved() const;
    int  moves() const { return moves_; }
    int  at(int cell) const;                        // tile value, 0 = hole, -1 if oob
    int  hole() const;                              // cell index of the hole
    std::vector<int> holeNeighbors() const;         // cells orthogonally adjacent to the hole

private:
    int board_[COUNT] = {};
    int moves_ = 0;
    static bool adjacent(int a, int b);
};

} // namespace paperos
