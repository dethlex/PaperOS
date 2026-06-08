#pragma once
#include "apps/games/IGame.h"
#include "apps/games/core/FifteenCore.h"
#include "i18n/Strings.h"
#include <cstdint>

namespace paperos {

class FifteenPuzzle : public IGame {
public:
    void reset() override;
    void onInput(const InputEvent& e) override;
    void render(M5EPD_Canvas& c) override;
    const char* title() const override { return tr(Str::game_fifteen); }
    const char* iconId() const override { return "game_fifteen"; }
    bool consumeFullRefresh() override { bool f = full_; full_ = false; return f; }

private:
    FifteenCore core_;
    bool full_ = true;
    bool solved_ = false;
    uint32_t start_ms_ = 0;
    uint32_t solve_ms_ = 0;
};

} // namespace paperos
