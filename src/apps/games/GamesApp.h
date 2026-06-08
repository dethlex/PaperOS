#pragma once
#include "framework/App.h"
#include "apps/games/IGame.h"
#include "apps/games/TicTacToe.h"
#include "apps/games/Minesweeper.h"
#include "apps/games/FifteenPuzzle.h"
#include "apps/games/Sudoku.h"
#include "i18n/Strings.h"

namespace paperos {

// Launcher app «Игры»: a tappable menu of four games (Menu) and the active game
// (InGame). G38/onBack steps InGame->Menu->Launcher. Games are held as members
// (no heap churn); current_ points at the active one.
class GamesApp : public IApp {
public:
    void enter(AppContext& ctx) override;
    void onInput(const InputEvent& e, AppContext& ctx) override;
    bool onBack(AppContext& ctx) override;
    const char* id() const override { return "games"; }
    const char* title() const override { return tr(Str::app_games); }

private:
    static constexpr int kGameCount = 4;
    static constexpr int kRowH = 120;

    enum class Mode { Menu, InGame };
    Mode mode_ = Mode::Menu;

    TicTacToe     ttt_;
    Minesweeper   mines_;
    FifteenPuzzle fifteen_;
    Sudoku        sudoku_;
    IGame* games_[kGameCount] = { &ttt_, &mines_, &fifteen_, &sudoku_ };
    IGame* current_ = nullptr;
    Rect rowRect(int i) const;
    void renderMenu(AppContext& ctx);
    void renderGame(AppContext& ctx, bool full);
};

} // namespace paperos
