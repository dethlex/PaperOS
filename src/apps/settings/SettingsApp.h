#pragma once
#include "framework/App.h"
#include "framework/ui/Keyboard.h"
#include "i18n/Strings.h"
#include "services/CalendarData.h"
#include <string>
#include <vector>

namespace paperos {

class SettingsApp : public IApp {
public:
    void enter(AppContext& ctx) override;
    void onInput(const InputEvent& e, AppContext& ctx) override;
    bool onBack(AppContext& ctx) override;
    const char* id() const override { return "settings"; }
    const char* title() const override { return tr(Str::app_settings); }

private:
    enum class Section {
        Index,
        Wifi, Ha, Reader, Time, Screensaver, Weather, Calendar, Language, About
    };
    Section section_ = Section::Index;

    Keyboard kb_;
    std::string editing_field_;  // "wifi_ssid", "wifi_pass", "ha_url", "ha_token", "wx_lat", "wx_lon"

    // Per-section "show secret" toggles — when on, mask is disabled for that
    // section's password/token row so the user can verify what they typed.
    bool show_wifi_pass_ = false;
    bool show_ha_token_  = false;

    struct Row { std::string label; std::string value; std::string field_key; bool mask; };
    std::vector<Row> rows_;

    bool picking_calendar_ = false;            // открыт пикер сущностей
    std::vector<CalendarInfo> pick_list_;      // ответ /api/calendars

    void renderIndex(AppContext& ctx);
    void renderSection(AppContext& ctx);
    void onIndexTouch(int16_t x, int16_t y, AppContext& ctx);
    void onSectionTouch(int16_t x, int16_t y, AppContext& ctx);
    void startEdit(const std::string& field_key, const std::string& current, bool mask, AppContext& ctx);
    void applyEdit(const std::string& field_key, const std::string& new_value, AppContext& ctx);
    void rebuildRows(AppContext& ctx);
    void wifiTest(AppContext& ctx);
    void haTest(AppContext& ctx);
    void weatherUpdate(AppContext& ctx);
    void calendarPick(AppContext& ctx);        // загрузить список + открыть пикер
    void renderCalendarPicker(AppContext& ctx);
    void calendarTest(AppContext& ctx);        // fetch + toast с числом событий
};

} // namespace paperos
