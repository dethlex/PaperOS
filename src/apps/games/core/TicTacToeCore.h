#pragma once
#include <cstdint>

namespace paperos {

// Pure 3x3 tic-tac-toe board + unbeatable minimax. No M5 deps (native-tested).
class TicTacToeCore {
public:
    enum class Cell   : uint8_t { Empty, X, O };
    enum class Result : uint8_t { None, X, O, Draw };

    void   reset();
    bool   place(int idx, Cell mark);   // false if idx invalid / occupied / game already over
    Cell   at(int idx) const;
    Result result() const;              // None until win or full board
    bool   gameOver() const;            // result() != None
    int    bestMove(Cell mark) const;   // minimax; -1 if no legal move / game over
    static Result winnerOf(const Cell b[9]);

private:
    Cell board_[9] = {};
    // Score of position for maximizing player `me`, with `toMove` to move.
    // Depth weighting makes it prefer faster wins / slower losses.
    static int scoreFor(Cell b[9], Cell toMove, Cell me, int depth);
};

} // namespace paperos
