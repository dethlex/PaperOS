#pragma once
#include "framework/App.h"
#include "apps/ha/Dashboard.h"
#include "services/HAClient.h"
#include "i18n/Strings.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace paperos {

class HomeAssistantApp : public IApp {
public:
    void enter(AppContext& ctx) override;
    void leave(AppContext& ctx) override;
    void tick(AppContext& ctx) override;
    void onInput(const InputEvent& e, AppContext& ctx) override;
    const char* id() const override { return "ha"; }
    const char* title() const override { return tr(Str::app_ha); }

private:
    Dashboard dashboard_;
    std::unordered_map<std::string, EntityState> states_;
    bool offline_ = true;
    uint32_t last_poll_ms_ = 0;
    int scroll_offset_ = 0;
    int content_bottom_ = 0;   // last rendered y extent, for scroll clamping

    // Incremental polling: each getState() is a blocking HTTPS round-trip
    // (TLS handshake ~1-2s). Sweeping every entity in one tick() blocked the
    // single UI thread long enough to starve button/touch input — the whole
    // device felt frozen while online. So we poll ONE entity per tick(),
    // round-robin, and only re-render once a full round finishes (and only if
    // something actually changed).
    std::vector<std::string> poll_ids_;
    size_t poll_cursor_ = 0;
    bool round_active_ = false;
    bool round_changed_ = false;

    // After a tap we flip the state optimistically for instant feedback, then
    // re-query that one entity a beat later to confirm/correct it (the device
    // needs a moment to actually change; an immediate re-query often returns
    // the pre-toggle value for async integrations).
    std::string confirm_id_;
    bool confirm_pending_ = false;
    uint32_t confirm_at_ms_ = 0;
    static constexpr uint32_t kConfirmDelayMs = 1500;

    struct CardRect { std::string entity_id; WidgetKind kind; Rect bounds; };
    std::vector<CardRect> card_rects_;

    void buildPollList();
    void seedFromCache(AppContext& ctx);
    bool pollOne(AppContext& ctx, const std::string& id);   // true if state changed
    void render(AppContext& ctx);
    void scrollBy(int dir, AppContext& ctx);
    void onCardTap(const CardRect& cr, AppContext& ctx);
};

} // namespace paperos
