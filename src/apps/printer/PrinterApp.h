#pragma once
#include "framework/App.h"
#include "services/PrinterStatus.h"
#include "framework/ui/Geometry.h"
#include "i18n/Strings.h"
#include "apps/printer/ConfirmDialog.h"
#include <cstdint>
#include <vector>

namespace paperos {

class PrinterApp : public IApp {
public:
    void enter(AppContext& ctx) override;
    void leave(AppContext& ctx) override;
    void tick(AppContext& ctx) override;
    void onInput(const InputEvent& e, AppContext& ctx) override;
    bool onBack(AppContext& ctx) override;
    bool keepAwake() const override { return computeMode() == Mode::Printing; }
    const char* id() const override { return "printer"; }
    const char* title() const override { return tr(Str::app_printer); }

private:
    enum class Mode { Printing, Idle };
    Mode computeMode() const;

    PrinterStatus status_;
    PowerState power_ = PowerState::Unknown;          // last known power state
    PowerState power_drawn_ = PowerState::Unknown;    // power state at last render
    std::vector<HistoryEntry> history_;
    bool history_loaded_ = false;
    bool has_power_dev_ = false;       // power_device configured
    int  preheat_nozzle_ = 200;       // cached config temps
    int  preheat_bed_ = 60;
    int  tz_offset_ = 0;              // timezone offset in hours, cached from config

    uint32_t slicer_est_s_ = 0;     // slicer estimated_time for current file (0 = none)
    std::string slicer_est_file_;   // filename slicer_est_s_ belongs to
    uint32_t gcode_start_ = 0, gcode_end_ = 0;   // print-body byte range for the current file

    std::vector<uint8_t> thumb_gray_;   // decoded thumbnail luminance (empty = none)
    int thumb_w_ = 0, thumb_h_ = 0;

    bool offline_    = true;
    bool no_url_     = false;
    bool got_first_  = false;
    uint32_t last_poll_ms_ = 0;

    // Optimistic re-poll (light/power/start), mirrors the existing light idiom.
    bool confirm_pending_ = false;
    uint32_t confirm_at_ms_ = 0;

    // Brief "action failed" banner.
    uint32_t failed_until_ms_ = 0;

    // Optimistic Printing override after a successful startPrint(), with deadline.
    bool start_override_ = false;
    uint32_t start_override_until_ms_ = 0;

    ConfirmDialog dialog_;

    // Idle scroll state.
    int scroll_offset_ = 0;
    int content_bottom_ = 0;
    Rect power_btn_{0, 0, 0, 0};

    std::string render_sig_;  // layout signature of the last pushed frame

    static constexpr int kJogStepMm = 10;
    static constexpr int kJogFeedXY = 3000;
    static constexpr int kJogFeedZ  = 600;

    Rect jog_[7]{};        // 0:X- 1:X+ 2:Y- 3:Y+ 4:Z- 5:Z+ 6:Home
    Rect preheat_btn_{0, 0, 0, 0};
    Rect cooldown_btn_{0, 0, 0, 0};
    Rect restart_btn_{0, 0, 0, 0};

    Rect hist_rows_[8]{};
    int  drawHistorySection(M5EPD_Canvas& c, int y);

    int  drawMoveSection(M5EPD_Canvas& c, int y);
    int  drawPreheatSection(M5EPD_Canvas& c, int y);
    void onJogTap(AppContext& ctx, int idx);

    // Hit rects (recomputed each render).
    Rect light_btn_{0, 0, 0, 0};
    Rect cancel_btn_{0, 0, 0, 0};
    Rect pause_btn_{0, 0, 0, 0};

    static constexpr uint32_t kPollIntervalMs = 3000;       // while printing (or start-override)
    static constexpr uint32_t kIdlePollIntervalMs = 30000;  // idle: slow poll, no 3s screen churn
    static constexpr uint32_t kConfirmDelayMs = 1500;
    static constexpr uint32_t kFailedBannerMs = 2500;
    static constexpr uint32_t kStartOverrideMs = 6000;
    static constexpr size_t   kHistoryShow = 8;

    void seedFromCache(AppContext& ctx);
    bool doPoll(AppContext& ctx);
    void refreshPower(AppContext& ctx);
    void refreshHistory(AppContext& ctx);
    void render(AppContext& ctx);
    void renderPrinting(AppContext& ctx);
    void renderIdle(AppContext& ctx);

    void scrollBy(int dir, AppContext& ctx);
    int  drawStatusBand(M5EPD_Canvas& c, int y);
    void onPowerTap(AppContext& ctx);
    void onLightTap(AppContext& ctx);
    void runConfirmed(AppContext& ctx, ConfirmKind kind, const std::string& payload);
    void noteFailed(AppContext& ctx);
};

} // namespace paperos
