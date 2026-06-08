#include "apps/games/core/SudokuCore.h"

namespace paperos {

// Six baked easy puzzles. Each is one validity-preserving family member (a valid
// complete solution masked to ~43 givens, with per-puzzle digit relabeling), so
// givens are always conflict-free and completable. '0' = empty.
static const char* kPuzzles[] = {
    "120400709056089020709003406030560800507090204001034060305600902070910340902005078",
    "230500801067091030801004507040670900608010305002045070406700103080120450103006089",
    "340600902078012040902005608050780100709020406003056080507800204090230560204007091",
    "450700103089023050103006709060890200801030507004067090608900305010340670305008012",
    "560800204091034060204007801070910300902040608005078010709100406020450780406009023",
    "670900305012045070305008902080120400103050709006089020801200507030560890507001034",
};

static constexpr int kPuzzleCount = sizeof(kPuzzles) / sizeof(kPuzzles[0]);

int         SudokuCore::puzzleCount()      { return kPuzzleCount; }
const char* SudokuCore::puzzle(int i)      { return kPuzzles[((i % kPuzzleCount) + kPuzzleCount) % kPuzzleCount]; }

// REQUIRES: puzzle == nullptr, or points to at least COUNT (81) chars. All
// current callers pass baked 81-char literals or puzzle(i). Reads COUNT chars
// with no length check by design (no runtime string scan on device).
void SudokuCore::load(const char* puzzle) {
    for (int i = 0; i < COUNT; ++i) {
        char ch = puzzle ? puzzle[i] : '0';
        if (ch >= '1' && ch <= '9') { val_[i] = static_cast<uint8_t>(ch - '0'); given_[i] = true; }
        else                         { val_[i] = 0;                              given_[i] = false; }
    }
}

bool SudokuCore::isGiven(int r, int c) const { return valid(r, c) && given_[idx(r, c)]; }
int  SudokuCore::at(int r, int c) const      { return valid(r, c) ? val_[idx(r, c)] : 0; }

bool SudokuCore::set(int r, int c, int v) {
    if (!valid(r, c) || v < 1 || v > 9) return false;
    if (given_[idx(r, c)]) return false;
    val_[idx(r, c)] = static_cast<uint8_t>(v);
    return true;
}

bool SudokuCore::clear(int r, int c) {
    if (!valid(r, c) || given_[idx(r, c)]) return false;
    val_[idx(r, c)] = 0;
    return true;
}

bool SudokuCore::hasConflict(int r, int c) const {
    if (!valid(r, c)) return false;
    int v = val_[idx(r, c)];
    if (v == 0) return false;
    for (int cc = 0; cc < N; ++cc)
        if (cc != c && val_[idx(r, cc)] == v) return true;
    for (int rr = 0; rr < N; ++rr)
        if (rr != r && val_[idx(rr, c)] == v) return true;
    int br = (r / 3) * 3, bc = (c / 3) * 3;
    for (int rr = br; rr < br + 3; ++rr)
        for (int cc = bc; cc < bc + 3; ++cc)
            if ((rr != r || cc != c) && val_[idx(rr, cc)] == v) return true;
    return false;
}

bool SudokuCore::isSolved() const {
    for (int i = 0; i < COUNT; ++i) if (val_[i] == 0) return false;
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c)
            if (hasConflict(r, c)) return false;
    return true;
}

} // namespace paperos
