#include "HomeAssistantApp.h"
#include "hal/Display.h"
#include "framework/ui/Fonts.h"
#include "services/ConfigStore.h"
#include "util/Logger.h"
#include "i18n/Strings.h"
#include <Arduino.h>

namespace paperos {

void HomeAssistantApp::enter(AppContext& ctx) {
    dashboard_.loadFromSd("/paperos/ha_dashboard.json");
    states_.clear();
    card_rects_.clear();
    scroll_offset_ = 0;
    buildPollList();
    seedFromCache(ctx);                  // show last-known values instantly
    ctx.ha.begin();
    offline_ = !ctx.ha.ensureWifi();     // one-time WiFi connect (may block ~8s)
    render(ctx);                         // draw immediately (cached or empty)
    // Live values are fetched incrementally by tick(); last_poll_ms_=0 makes
    // the first round start on the next tick.
    round_active_ = false;
    poll_cursor_ = 0;
    last_poll_ms_ = 0;
    confirm_pending_ = false;
}

void HomeAssistantApp::leave(AppContext& ctx) {
    ctx.ha.releaseWifi();
}

void HomeAssistantApp::tick(AppContext& ctx) {
    uint32_t now = millis();

    // Pending post-tap confirmation: re-query the toggled entity once the
    // device has had ~kConfirmDelayMs to actually change, then correct the
    // optimistic value if it differs. Done before the round logic and as the
    // only work this tick.
    if (confirm_pending_ && static_cast<int32_t>(now - confirm_at_ms_) >= 0) {
        confirm_pending_ = false;
        if (!offline_ && pollOne(ctx, confirm_id_)) render(ctx);
        return;
    }

    if (offline_) return;
    if (!round_active_) {
        if (now - last_poll_ms_ < 10000) return;   // gap between refresh rounds
        round_active_ = true;
        poll_cursor_ = 0;
        round_changed_ = false;
    }
    // Fetch exactly ONE entity this tick — keeps each loop iteration short so
    // buttons/touch are sampled between fetches instead of being starved by a
    // full multi-entity HTTPS sweep.
    if (poll_cursor_ < poll_ids_.size()) {
        if (pollOne(ctx, poll_ids_[poll_cursor_])) round_changed_ = true;
        poll_cursor_++;
    }
    if (poll_cursor_ >= poll_ids_.size()) {         // round complete
        round_active_ = false;
        last_poll_ms_ = now;
        if (round_changed_) render(ctx);            // refresh only if changed
    }
}

void HomeAssistantApp::buildPollList() {
    poll_ids_.clear();
    for (const auto& g : dashboard_.groups())
        for (const auto& it : g.items)
            poll_ids_.push_back(it.entity_id);
}

void HomeAssistantApp::seedFromCache(AppContext& ctx) {
    for (const auto& id : poll_ids_) {
        char buf[64];
        size_t n = ctx.config.getEntityState(id.c_str(), buf, sizeof(buf));
        if (n > 0) {
            EntityState cs;
            cs.entity_id = id;
            cs.state.assign(buf, n);
            states_[id] = cs;
        }
    }
}

bool HomeAssistantApp::pollOne(AppContext& ctx, const std::string& id) {
    EntityState s;
    if (!ctx.ha.getState(id, s)) return false;   // transient failure — keep old value
    auto it = states_.find(id);
    bool changed = (it == states_.end() ||
                    it->second.state != s.state ||
                    it->second.unit  != s.unit);
    states_[id] = s;
    ctx.config.putEntityState(id.c_str(), s.state.data(), s.state.size());
    return changed;
}

static std::pair<std::string, std::string> splitEntity(const std::string& eid) {
    auto p = eid.find('.');
    if (p == std::string::npos) return {eid, ""};
    return { eid.substr(0, p), eid.substr(p + 1) };
}

void HomeAssistantApp::render(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts; fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.setTextColor(15);
    c.drawString(tr(Str::app_ha), kHeaderTextX, kHeaderTextY);
    if (offline_) {
        fonts.apply(c, FontFace::Serif, 18);
        c.drawString(tr(Str::weather_offline), kScreenW - 100, kHeaderTextY + 6);
    }

    card_rects_.clear();
    int16_t y = 80 - scroll_offset_;
    fonts.apply(c, FontFace::Serif, 22);

    // Empty-dashboard hint — surfaces the missing/invalid config file instead
    // of leaving the user staring at just the header.
    if (dashboard_.groups().empty()) {
        c.drawString(tr(Str::ha_no_dashboard), 20, y);
        y += 30;
        c.drawString(tr(Str::ha_invalid), 20, y);
        ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
        return;
    }

    for (const auto& g : dashboard_.groups()) {
        // Group sub-header — set the font EXPLICITLY every iteration. Otherwise
        // the title inherits whatever apply() ran last (the 36px card value of
        // the previous group), so the first group looked small and the rest
        // huge. Consistent 26px + a thin separator under it.
        fonts.apply(c, FontFace::Serif, 26);
        c.setTextColor(15);
        c.drawString(g.title.c_str(), 20, y);
        c.drawLine(20, y + 34, kScreenW - 20, y + 34, 8);
        y += 46;
        for (const auto& it : g.items) {
            Rect r{20, y, kScreenW - 40, 110};
            c.drawRect(r.x, r.y, r.w, r.h, 12);
            fonts.apply(c, FontFace::Serif, 22);
            c.drawString(it.label.c_str(), r.x + 16, r.y + 12);

            std::string val = "\xe2\x80\x94"; // U+2014 EM DASH in UTF-8
            auto s = states_.find(it.entity_id);
            if (s != states_.end()) val = s->second.state;
            if (it.kind == WidgetKind::Sensor && !it.unit.empty()) val += " " + it.unit;
            // Lock widgets show a human-readable, tappable status.
            if (it.kind == WidgetKind::Lock) {
                if      (val == "locked")   val = tr(Str::ha_locked);
                else if (val == "unlocked") val = tr(Str::ha_unlocked);
                // (other states like "jammed"/"unknown" pass through raw)
            }

            fonts.apply(c, FontFace::Serif, 36);
            c.drawString(val.c_str(), r.x + 16, r.y + 50);

            card_rects_.push_back({it.entity_id, it.kind, r});
            y += 120;
        }
        y += 10;
    }

    content_bottom_ = y + scroll_offset_;   // absolute bottom (undo the scroll shift)

    // Scroll indicator on the right edge — shows position and that more content
    // exists below/above. Only drawn when content overflows the viewport.
    {
        const int visible_h = kScreenH - 80;
        const int content_h = content_bottom_ - 80;
        if (content_h > visible_h) {
            const int track_x = kScreenW - 12;
            const int track_y = 80;
            c.drawRect(track_x, track_y, 8, visible_h, 12);
            int thumb_h = visible_h * visible_h / content_h;
            if (thumb_h < 24) thumb_h = 24;
            const int span = content_h - visible_h;
            int thumb_y = track_y + (visible_h - thumb_h) *
                          (span > 0 ? scroll_offset_ : 0) / (span > 0 ? span : 1);
            c.fillRect(track_x, thumb_y, 8, thumb_h, 15);
        }
    }
    ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
}

void HomeAssistantApp::onCardTap(const CardRect& cr, AppContext& ctx) {
    if (!ctx.ha.ensureWifi()) return;
    auto [domain, name] = splitEntity(cr.entity_id);
    (void)name;
    bool ok = false;
    bool stateful = false;   // toggle/lock have a state to confirm; actions don't
    if (cr.kind == WidgetKind::Toggle) {
        ok = ctx.ha.callService(domain, "toggle", cr.entity_id);
        if (ok) {
            auto& s = states_[cr.entity_id];
            s.entity_id = cr.entity_id;
            s.state = (s.state == "on") ? "off" : "on";   // optimistic flip
            stateful = true;
        }
    } else if (cr.kind == WidgetKind::Action) {
        ok = ctx.ha.callService(domain, "turn_on", cr.entity_id);
    } else if (cr.kind == WidgetKind::Lock) {
        // The lock domain has no toggle service — pick lock/unlock based on
        // the current cached state (default to locking if state is unknown).
        auto& s = states_[cr.entity_id];
        bool locked = (s.state == "locked");
        ok = ctx.ha.callService(domain, locked ? "unlock" : "lock", cr.entity_id);
        if (ok) {
            s.entity_id = cr.entity_id;
            s.state = locked ? "unlocked" : "locked";   // optimistic flip
            stateful = true;
        }
    }
    if (ok) {
        if (stateful) {                       // schedule a confirming re-query
            confirm_id_ = cr.entity_id;
            confirm_pending_ = true;
            confirm_at_ms_ = millis() + kConfirmDelayMs;
        }
        render(ctx);
    }
}

void HomeAssistantApp::onInput(const InputEvent& e, AppContext& ctx) {
    if (e.kind == InputEvent::Kind::NavUp)   { scrollBy(-1, ctx); return; }
    if (e.kind == InputEvent::Kind::NavDown) { scrollBy(+1, ctx); return; }
    if (e.kind != InputEvent::Kind::TouchUp) return;
    for (const auto& cr : card_rects_) {
        if (cr.bounds.contains({e.x, e.y})) { onCardTap(cr, ctx); return; }
    }
}

void HomeAssistantApp::scrollBy(int dir, AppContext& ctx) {
    const int visible_h = kScreenH - 80;          // content starts at y=80
    const int content_h = content_bottom_ - 80;   // height of rendered content
    const int max_off = (content_h > visible_h) ? (content_h - visible_h) : 0;
    const int step = (visible_h - 120) * dir;      // one screenful minus one card
    int next = scroll_offset_ + step;
    if (next < 0) next = 0;
    if (next > max_off) next = max_off;
    if (next == scroll_offset_) return;            // at edge — no refresh
    scroll_offset_ = next;
    render(ctx);
}

} // namespace paperos
