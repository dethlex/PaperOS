#pragma once
#include "apps/games/IGame.h"
#include "apps/games/core/MinesweeperCore.h"
#include "i18n/Strings.h"

namespace paperos {

class Minesweeper : public IGame {
public:
    void reset() override;
    void onInput(const InputEvent& e) override;
    void render(M5EPD_Canvas& c) override;
    const char* title() const override { return tr(Str::game_minesweeper); }
    const char* iconId() const override { return "game_minesweeper"; }
    bool consumeFullRefresh() override { bool f = full_; full_ = false; return f; }

private:
    MinesweeperCore core_;
    bool flag_mode_ = false;     // false = reveal, true = flag
    bool full_ = true;
};

} // namespace paperos
