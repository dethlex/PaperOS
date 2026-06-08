#pragma once
#include <cstdint>

namespace paperos {

// Pure 9x9 sudoku. Loads a baked 81-char puzzle ('1'-'9' = given, anything else
// = empty). Win = all filled with zero row/col/box conflicts (no stored solution,
// no uniqueness guarantee — MVP). No M5 deps (native-tested).
class SudokuCore {
public:
    static constexpr int N = 9;
    static constexpr int COUNT = 81;

    void load(const char* puzzle);          // reads 81 chars; givens locked
    bool isGiven(int r, int c) const;
    int  at(int r, int c) const;            // 0 = empty
    bool set(int r, int c, int v);          // v in 1..9; false if given/oob
    bool clear(int r, int c);               // false if given/oob
    bool hasConflict(int r, int c) const;   // value duplicated in row/col/box
    bool isSolved() const;                  // all filled AND no conflicts

    static int         puzzleCount();
    static const char* puzzle(int i);       // baked given string (i wraps)

private:
    uint8_t val_[COUNT]  = {};
    bool    given_[COUNT] = {};
    static int  idx(int r, int c) { return r * N + c; }
    static bool valid(int r, int c) { return r >= 0 && r < N && c >= 0 && c < N; }
};

} // namespace paperos
