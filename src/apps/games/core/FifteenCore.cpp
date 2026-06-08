#include "apps/games/core/FifteenCore.h"

namespace paperos {

void FifteenCore::reset() {
    for (int i = 0; i < 15; ++i) board_[i] = i + 1;
    board_[15] = 0;
    moves_ = 0;
}

int FifteenCore::at(int cell) const {
    return (cell >= 0 && cell < COUNT) ? board_[cell] : -1;
}

int FifteenCore::hole() const {
    for (int i = 0; i < COUNT; ++i) if (board_[i] == 0) return i;
    return -1;
}

bool FifteenCore::adjacent(int a, int b) {
    int ax = a % N, ay = a / N, bx = b % N, by = b / N;
    int dx = ax - bx, dy = ay - by;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx + dy) == 1;
}

std::vector<int> FifteenCore::holeNeighbors() const {
    int h = hole();
    std::vector<int> out;
    for (int i = 0; i < COUNT; ++i) if (adjacent(i, h)) out.push_back(i);
    return out;
}

bool FifteenCore::slide(int cell) {
    if (cell < 0 || cell >= COUNT) return false;
    int h = hole();
    if (!adjacent(cell, h)) return false;
    board_[h] = board_[cell];
    board_[cell] = 0;
    ++moves_;
    return true;
}

void FifteenCore::shuffle(const std::vector<int>& moveCells) {
    for (int c : moveCells) slide(c);
    moves_ = 0;
}

bool FifteenCore::isSolved() const {
    for (int i = 0; i < COUNT - 1; ++i) if (board_[i] != i + 1) return false;
    return board_[COUNT - 1] == 0;
}

} // namespace paperos
