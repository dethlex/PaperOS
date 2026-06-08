#include <unity.h>
#include <cstring>
#include "apps/games/core/SudokuCore.h"

using namespace paperos;
void setUp() {} void tearDown() {}

static const char* kSolved =
    "123456789456789123789123456234567891567891234891234567345678912678912345912345678";

void test_load_givens_and_at() {
    SudokuCore s; s.load(SudokuCore::puzzle(0));
    TEST_ASSERT_TRUE (s.isGiven(0, 0));    // puzzle 0 starts with '1'
    TEST_ASSERT_EQUAL_INT(1, s.at(0, 0));
    TEST_ASSERT_FALSE(s.isGiven(0, 2));    // a blank
    TEST_ASSERT_EQUAL_INT(0, s.at(0, 2));
}

void test_set_clear_non_given() {
    SudokuCore s; s.load(SudokuCore::puzzle(0));
    TEST_ASSERT_TRUE(s.set(0, 2, 3));
    TEST_ASSERT_EQUAL_INT(3, s.at(0, 2));
    TEST_ASSERT_TRUE(s.clear(0, 2));
    TEST_ASSERT_EQUAL_INT(0, s.at(0, 2));
}

void test_given_is_locked() {
    SudokuCore s; s.load(SudokuCore::puzzle(0));
    TEST_ASSERT_FALSE(s.set(0, 0, 9));     // (0,0) is a given
    TEST_ASSERT_EQUAL_INT(1, s.at(0, 0));
    TEST_ASSERT_FALSE(s.clear(0, 0));
    TEST_ASSERT_EQUAL_INT(1, s.at(0, 0));
}

void test_conflict_detection() {
    SudokuCore s; s.load(SudokuCore::puzzle(0));
    TEST_ASSERT_TRUE(s.set(0, 2, 1));      // (0,0) already 1 -> row conflict
    TEST_ASSERT_TRUE(s.hasConflict(0, 2));
    TEST_ASSERT_TRUE(s.clear(0, 2));
    TEST_ASSERT_TRUE(s.set(0, 2, 3));      // 3 is free in row/col/box here
    TEST_ASSERT_FALSE(s.hasConflict(0, 2));
}

void test_solved_grid() {
    SudokuCore s; s.load(kSolved);
    TEST_ASSERT_TRUE(s.isSolved());
}

void test_puzzle_not_solved_and_has_blanks() {
    SudokuCore s; s.load(SudokuCore::puzzle(0));
    TEST_ASSERT_FALSE(s.isSolved());
}

void test_all_baked_puzzles_valid() {
    TEST_ASSERT_EQUAL_INT(6, SudokuCore::puzzleCount());
    for (int i = 0; i < SudokuCore::puzzleCount(); ++i) {
        const char* p = SudokuCore::puzzle(i);
        TEST_ASSERT_EQUAL_INT(81, (int)strlen(p));
        SudokuCore s; s.load(p);
        int givens = 0;
        for (int r = 0; r < 9; ++r)
            for (int c = 0; c < 9; ++c)
                if (s.isGiven(r, c)) {
                    ++givens;
                    TEST_ASSERT_FALSE(s.hasConflict(r, c));   // no contradictory givens
                }
        TEST_ASSERT_TRUE(givens > 0 && givens < 81);          // has blanks to fill
    }
}

void test_col_conflict() {
    SudokuCore s; s.load(SudokuCore::puzzle(0));
    // (3,8) is empty; 9 already sits at (0,8) in the same column,
    // and 9 is absent from row 3 and from this 3x3 box -> column-only conflict.
    TEST_ASSERT_TRUE(s.set(3, 8, 9));
    TEST_ASSERT_TRUE(s.hasConflict(3, 8));
}

void test_box_conflict() {
    SudokuCore s; s.load(SudokuCore::puzzle(0));
    // (1,3) is empty; 3 already sits at (2,5) in the same 3x3 box,
    // and 3 is absent from row 1 and from column 3 -> box-only conflict.
    TEST_ASSERT_TRUE(s.set(1, 3, 3));
    TEST_ASSERT_TRUE(s.hasConflict(1, 3));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_load_givens_and_at);
    RUN_TEST(test_set_clear_non_given);
    RUN_TEST(test_given_is_locked);
    RUN_TEST(test_conflict_detection);
    RUN_TEST(test_solved_grid);
    RUN_TEST(test_puzzle_not_solved_and_has_blanks);
    RUN_TEST(test_all_baked_puzzles_valid);
    RUN_TEST(test_col_conflict);
    RUN_TEST(test_box_conflict);
    return UNITY_END();
}
