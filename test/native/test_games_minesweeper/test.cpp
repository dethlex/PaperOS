#include <unity.h>
#include "apps/games/core/MinesweeperCore.h"

using namespace paperos;
using State = MinesweeperCore::State;
void setUp() {} void tearDown() {}

void test_adjacency_counts() {
    MinesweeperCore m; m.reset();
    m.placeMines(63, {0});                 // single mine at (0,0)
    TEST_ASSERT_EQUAL_INT(1, m.adjacent(1, 0));
    TEST_ASSERT_EQUAL_INT(1, m.adjacent(0, 1));
    TEST_ASSERT_EQUAL_INT(1, m.adjacent(1, 1));
    TEST_ASSERT_EQUAL_INT(0, m.adjacent(2, 0));
}

void test_first_cell_excluded_from_mines() {
    MinesweeperCore m; m.reset();
    m.placeMines(0, {0, 1, 2});            // 0 is the safe first cell -> filtered out
    TEST_ASSERT_FALSE(m.isMine(0, 0));
    TEST_ASSERT_TRUE (m.isMine(1, 0));
    TEST_ASSERT_TRUE (m.isMine(2, 0));
}

void test_flood_opens_region_and_wins() {
    MinesweeperCore m; m.reset();
    m.placeMines(63, {0});                 // one mine in the corner
    m.reveal(4, 4);                        // a zero cell: floods the whole safe board
    TEST_ASSERT_TRUE (m.isRevealed(7, 7));
    TEST_ASSERT_TRUE (m.isRevealed(1, 1)); // border number cell, still revealed
    TEST_ASSERT_FALSE(m.isRevealed(0, 0)); // the mine
    TEST_ASSERT_EQUAL(State::Won, m.state());  // all 63 safe cells revealed
}

void test_reveal_mine_loses() {
    MinesweeperCore m; m.reset();
    m.placeMines(63, {0});
    m.reveal(0, 0);
    TEST_ASSERT_EQUAL(State::Lost, m.state());
    TEST_ASSERT_TRUE(m.isRevealed(0, 0));
}

void test_flag_toggle_and_remaining() {
    MinesweeperCore m; m.reset();
    m.placeMines(63, {0});
    TEST_ASSERT_EQUAL_INT(10, m.minesRemaining());
    m.toggleFlag(2, 2);
    TEST_ASSERT_TRUE(m.isFlagged(2, 2));
    TEST_ASSERT_EQUAL_INT(9, m.minesRemaining());
    m.toggleFlag(2, 2);
    TEST_ASSERT_FALSE(m.isFlagged(2, 2));
    TEST_ASSERT_EQUAL_INT(10, m.minesRemaining());
}

void test_flagged_cell_not_revealed() {
    MinesweeperCore m; m.reset();
    m.placeMines(63, {0});
    m.toggleFlag(4, 4);
    m.reveal(4, 4);                        // flagged -> reveal is a no-op
    TEST_ASSERT_FALSE(m.isRevealed(4, 4));
    TEST_ASSERT_EQUAL(State::Playing, m.state());
}

void test_reveal_after_loss_is_noop() {
    MinesweeperCore m; m.reset();
    m.placeMines(63, {0});
    m.reveal(0, 0);                    // hits mine -> Lost
    m.reveal(1, 0);                    // game over -> no-op
    TEST_ASSERT_FALSE(m.isRevealed(1, 0));
}

void test_placemines_ignores_out_of_range_positions() {
    MinesweeperCore m; m.reset();
    m.placeMines(63, {-1, 64, 100, 5});   // only 5 is a valid mine
    TEST_ASSERT_TRUE(m.isMine(5 % 8, 5 / 8));
    int mines = 0;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if (m.isMine(x, y)) ++mines;
    TEST_ASSERT_EQUAL_INT(1, mines);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_adjacency_counts);
    RUN_TEST(test_first_cell_excluded_from_mines);
    RUN_TEST(test_flood_opens_region_and_wins);
    RUN_TEST(test_reveal_mine_loses);
    RUN_TEST(test_flag_toggle_and_remaining);
    RUN_TEST(test_flagged_cell_not_revealed);
    RUN_TEST(test_reveal_after_loss_is_noop);
    RUN_TEST(test_placemines_ignores_out_of_range_positions);
    return UNITY_END();
}
