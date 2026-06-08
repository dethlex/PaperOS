#pragma once
#include "apps/games/IGame.h"
#include "apps/games/core/SudokuCore.h"
#include "i18n/Strings.h"

namespace paperos {

class Sudoku : public IGame {
public:
    void reset() override;
    void onInput(const InputEvent& e) override;
    void render(M5EPD_Canvas& c) override;
    const char* title() const override { return tr(Str::game_sudoku); }
    const char* iconId() const override { return "game_sudoku"; }
    bool consumeFullRefresh() override { bool f = full_; full_ = false; return f; }

private:
    SudokuCore core_;
    int sel_r_ = -1, sel_c_ = -1;
    bool full_ = true;
};

} // namespace paperos
