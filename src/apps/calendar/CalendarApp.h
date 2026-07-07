#pragma once
#include "framework/App.h"
#include "services/CalendarData.h"
#include "i18n/Strings.h"

namespace paperos {

class CalendarApp : public IApp {
public:
    void enter(AppContext& ctx) override;
    void leave(AppContext& ctx) override;
    void onInput(const InputEvent& e, AppContext& ctx) override;
    const char* id() const override { return "calendar"; }
    const char* title() const override { return tr(Str::app_calendar); }

private:
    void refresh(AppContext& ctx);   // WiFi + fetch + cache + render
    void render(AppContext& ctx);
    CalendarData data_;
    bool offline_ = false;
    bool configured_ = false;
    // Tap-cooldown anchor; intentionally NOT set by the gated enter() path — a
    // manual tap right after entry may force a fetch, which is the documented contract.
    uint32_t last_refresh_ms_ = 0;
};

} // namespace paperos
