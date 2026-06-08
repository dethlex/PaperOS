#include <unity.h>
#include "apps/games/core/TicTacToeCore.h"

using namespace paperos;
using Cell = TicTacToeCore::Cell;
using Result = TicTacToeCore::Result;
void setUp() {} void tearDown() {}

void test_row_win() {
    TicTacToeCore g; g.reset();
    g.place(0, Cell::X); g.place(3, Cell::O);
    g.place(1, Cell::X); g.place(4, Cell::O);
    g.place(2, Cell::X);                        // top row XXX
    TEST_ASSERT_EQUAL(Result::X, g.result());
    TEST_ASSERT_TRUE(g.gameOver());
}

void test_col_and_diag_win() {
    TicTacToeCore a; a.reset();
    a.place(0, Cell::O); a.place(1, Cell::X);
    a.place(3, Cell::O); a.place(2, Cell::X);
    a.place(6, Cell::O);                        // left column OOO
    TEST_ASSERT_EQUAL(Result::O, a.result());

    TicTacToeCore d; d.reset();
    d.place(0, Cell::X); d.place(1, Cell::O);
    d.place(4, Cell::X); d.place(2, Cell::O);
    d.place(8, Cell::X);                        // main diagonal XXX
    TEST_ASSERT_EQUAL(Result::X, d.result());
}

void test_draw_when_full_no_winner() {
    TicTacToeCore g; g.reset();
    // X O X / X O O / O X X  -> full, no line
    Cell layout[9] = {Cell::X, Cell::O, Cell::X,
                      Cell::X, Cell::O, Cell::O,
                      Cell::O, Cell::X, Cell::X};
    for (int i = 0; i < 9; ++i) TEST_ASSERT_TRUE(g.place(i, layout[i]));
    TEST_ASSERT_EQUAL(Result::Draw, g.result());
}

void test_place_rejects_occupied_and_after_over() {
    TicTacToeCore g; g.reset();
    TEST_ASSERT_TRUE (g.place(0, Cell::X));
    TEST_ASSERT_FALSE(g.place(0, Cell::O));    // occupied
    g.place(1, Cell::X); g.place(2, Cell::X);  // core doesn't enforce turn order — three X's on row 0 is legal here, X wins
    TEST_ASSERT_TRUE(g.gameOver());
    TEST_ASSERT_FALSE(g.place(4, Cell::O));    // game over
}

void test_minimax_takes_the_win() {
    TicTacToeCore g; g.reset();
    g.place(3, Cell::O); g.place(4, Cell::O);  // O at 3,4 -> winning move is 5
    TEST_ASSERT_EQUAL_INT(5, g.bestMove(Cell::O));
}

void test_minimax_blocks_opponent() {
    TicTacToeCore g; g.reset();
    g.place(0, Cell::X); g.place(1, Cell::X);  // X threatens 2; O must block at 2
    TEST_ASSERT_EQUAL_INT(2, g.bestMove(Cell::O));
}

void test_bestmove_minus_one_when_over() {
    TicTacToeCore g; g.reset();
    g.place(0, Cell::X); g.place(1, Cell::X); g.place(2, Cell::X);  // X already won
    TEST_ASSERT_TRUE(g.gameOver());
    TEST_ASSERT_EQUAL_INT(-1, g.bestMove(Cell::O));
}

void test_perfect_vs_perfect_is_draw() {
    TicTacToeCore g; g.reset();
    Cell turn = Cell::X;
    while (!g.gameOver()) {
        int m = g.bestMove(turn);
        TEST_ASSERT_TRUE(m >= 0);
        g.place(m, turn);
        turn = (turn == Cell::X) ? Cell::O : Cell::X;
    }
    TEST_ASSERT_EQUAL(Result::Draw, g.result());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_row_win);
    RUN_TEST(test_col_and_diag_win);
    RUN_TEST(test_draw_when_full_no_winner);
    RUN_TEST(test_place_rejects_occupied_and_after_over);
    RUN_TEST(test_minimax_takes_the_win);
    RUN_TEST(test_minimax_blocks_opponent);
    RUN_TEST(test_bestmove_minus_one_when_over);
    RUN_TEST(test_perfect_vs_perfect_is_draw);
    return UNITY_END();
}
