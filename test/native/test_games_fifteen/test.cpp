#include <unity.h>
#include "apps/games/core/FifteenCore.h"

using namespace paperos;
void setUp() {} void tearDown() {}

void test_reset_is_solved() {
    FifteenCore f; f.reset();
    TEST_ASSERT_TRUE(f.isSolved());
    TEST_ASSERT_EQUAL_INT(0, f.moves());
    TEST_ASSERT_EQUAL_INT(0, f.at(15));   // hole bottom-right
    TEST_ASSERT_EQUAL_INT(15, f.hole());
}

void test_legal_slide_moves_tile() {
    FifteenCore f; f.reset();
    TEST_ASSERT_TRUE(f.slide(14));        // tile 15 (at cell 14) slides into hole 15
    TEST_ASSERT_EQUAL_INT(15, f.at(15));
    TEST_ASSERT_EQUAL_INT(0,  f.at(14));
    TEST_ASSERT_EQUAL_INT(1,  f.moves());
    TEST_ASSERT_FALSE(f.isSolved());
}

void test_illegal_slide_ignored() {
    FifteenCore f; f.reset();
    TEST_ASSERT_FALSE(f.slide(0));        // cell 0 not adjacent to hole 15
    TEST_ASSERT_EQUAL_INT(0, f.moves());
    TEST_ASSERT_TRUE(f.isSolved());
}

void test_hole_neighbors() {
    FifteenCore f; f.reset();             // hole at 15 -> neighbors 11 (up) and 14 (left)
    auto n = f.holeNeighbors();
    TEST_ASSERT_EQUAL_INT(2, (int)n.size());
    bool has11 = false, has14 = false;
    for (int c : n) { if (c == 11) has11 = true; if (c == 14) has14 = true; }
    TEST_ASSERT_TRUE(has11);
    TEST_ASSERT_TRUE(has14);
}

void test_shuffle_is_permutation_and_resets_moves() {
    FifteenCore f; f.reset();
    f.shuffle({14, 13, 9, 10});           // legal chain from solved
    TEST_ASSERT_EQUAL_INT(0, f.moves());  // shuffle zeroes the counter
    bool seen[16] = {};
    for (int c = 0; c < 16; ++c) {
        int v = f.at(c);
        TEST_ASSERT_TRUE(v >= 0 && v < 16);
        TEST_ASSERT_FALSE(seen[v]);       // each value 0..15 exactly once
        seen[v] = true;
    }
}

void test_move_counter_counts_only_legal() {
    FifteenCore f; f.reset();
    f.slide(0);                           // illegal
    f.slide(14);                          // legal
    f.slide(0);                           // illegal (hole now at 14)
    TEST_ASSERT_EQUAL_INT(1, f.moves());
}

void test_no_row_wrap_adjacency() {
    FifteenCore f; f.reset();
    // walk the hole from 15 up to cell 4 (start of row 1) via legal slides
    for (int cell : {11, 7, 6, 5, 4}) TEST_ASSERT_TRUE(f.slide(cell));
    TEST_ASSERT_EQUAL_INT(4, f.hole());
    // cell 3 (end of row 0) is index-distance 1 from hole 4 but a DIFFERENT
    // row: it must NOT be a legal slide nor a hole-neighbor (no wrap-around).
    TEST_ASSERT_FALSE(f.slide(3));
    auto n = f.holeNeighbors();
    for (int c : n) TEST_ASSERT_TRUE(c != 3);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_reset_is_solved);
    RUN_TEST(test_legal_slide_moves_tile);
    RUN_TEST(test_illegal_slide_ignored);
    RUN_TEST(test_hole_neighbors);
    RUN_TEST(test_shuffle_is_permutation_and_resets_moves);
    RUN_TEST(test_move_counter_counts_only_legal);
    RUN_TEST(test_no_row_wrap_adjacency);
    return UNITY_END();
}
