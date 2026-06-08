#pragma once
#include "framework/App.h"
#include "i18n/Strings.h"
#include <vector>
#include <string>

namespace paperos {

class Launcher : public IApp {
public:
    void enter(AppContext& ctx) override;
    void onInput(const InputEvent& e, AppContext& ctx) override;
    void tick(AppContext& ctx) override;
    const char* id() const override { return "launcher"; }
    const char* title() const override { return tr(Str::app_home); }

private:
    struct Card { std::string app_id; std::string label; Rect bounds; };  // bounds in ABSOLUTE coords
    std::vector<Card> cards_;
    int last_minute_ = -1;
    int scroll_offset_ = 0;     // px scrolled down; 0 = top
    int content_bottom_ = 0;    // absolute y just below the last row (for scroll clamp)

    void renderAll(AppContext& ctx);
    void scrollBy(int dir, AppContext& ctx);
    // No instance state — operates purely on its args (matches the screensaver's
    // free-function draw helpers). Shared by renderAll (full) and tick (partial).
    static void drawStatus(M5EPD_Canvas& target, AppContext& ctx, int ox, int oy);
};

} // namespace paperos
