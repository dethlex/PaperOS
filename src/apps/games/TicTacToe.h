#pragma once
#include "apps/games/IGame.h"
#include "apps/games/core/TicTacToeCore.h"
#include "i18n/Strings.h"

namespace paperos {

class TicTacToe : public IGame {
public:
    void reset() override;
    void onInput(const InputEvent& e) override;
    void render(M5EPD_Canvas& c) override;
    const char* title() const override { return tr(Str::game_tictactoe); }
    const char* iconId() const override { return "game_tictactoe"; }
    bool consumeFullRefresh() override { bool f = full_; full_ = false; return f; }

private:
    TicTacToeCore core_;
    bool ai_ = true;                                    // AI vs 2-player hot-seat
    TicTacToeCore::Cell turn_ = TicTacToeCore::Cell::X; // current player in 2-player mode
    bool full_ = true;
};

} // namespace paperos
