#include "PrinterApp.h"
#include <Arduino.h>
#include <cstdio>
#include <string>
#include "hal/Display.h"
#include "services/ConfigStore.h"
#include "services/MoonrakerClient.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/IconKit.h"
#include "framework/ui/PngDraw.h"

namespace paperos {

namespace {
void fmtHM(uint32_t sec, char* out, size_t n) {
    unsigned h = sec / 3600, m = (sec % 3600) / 60;
    if (h > 0) std::snprintf(out, n, "%u:%02u", h, m);
    else       std::snprintf(out, n, "%u:%02u", m, static_cast<unsigned>(sec % 60));
}
int ireg(float t) { return static_cast<int>(t < 0 ? t - 0.5f : t + 0.5f); }
bool axisHomed(const std::string& homed, char axis) {
    char lo = static_cast<char>(axis | 0x20);   // tolower
    return homed.find(lo) != std::string::npos;
}
void jogBtn(M5EPD_Canvas& c, const Rect& r, const char* label, bool enabled) {
    c.drawRect(r.x, r.y, r.w, r.h, enabled ? 15 : 12);
    c.setTextColor(enabled ? 15 : 12);
    c.setTextDatum(MC_DATUM);
    c.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
    c.setTextDatum(TL_DATUM);
    c.setTextColor(15);
}
} // namespace

PrinterApp::Mode PrinterApp::computeMode() const {
    if (status_.state == PrinterStatus::PrintSt::Printing ||
        status_.state == PrinterStatus::PrintSt::Paused) return Mode::Printing;
    return Mode::Idle;
}

void PrinterApp::enter(AppContext& ctx) {
    ctx.printer.begin();
    has_power_dev_ = !ctx.printer.powerDevice().empty();
    preheat_nozzle_ = ctx.printer.preheatNozzle();
    preheat_bed_    = ctx.printer.preheatBed();
    tz_offset_      = ctx.config.tzOffsetHours();
    status_ = PrinterStatus{};
    power_ = power_drawn_ = PowerState::Unknown;
    history_.clear(); history_loaded_ = false;
    got_first_ = false; confirm_pending_ = false;
    start_override_ = false; failed_until_ms_ = 0;
    slicer_est_s_ = 0; slicer_est_file_.clear();
    gcode_start_ = gcode_end_ = 0;
    thumb_gray_.clear(); thumb_w_ = thumb_h_ = 0;
    last_poll_ms_ = 0;
    dialog_.close();

    no_url_ = !ctx.printer.configured();
    if (no_url_) { render(ctx); return; }

    seedFromCache(ctx);
    render(ctx);

    offline_ = !ctx.printer.ensureWifi();
    if (!offline_) {
        ctx.printer.discoverLightObject();
        ctx.printer.discoverPowerDevice();          // auto-discover when config empty
        has_power_dev_ = !ctx.printer.powerDevice().empty();   // recompute AFTER discovery
        doPoll(ctx);
        if (computeMode() == Mode::Idle) { refreshPower(ctx); refreshHistory(ctx); }
    }
    render(ctx);
}

void PrinterApp::leave(AppContext& ctx) {
    ctx.printer.releaseWifi();
}

void PrinterApp::seedFromCache(AppContext& ctx) {
    PrinterCache c{};
    size_t n = ctx.config.getPrinterCache(&c, sizeof(c));
    if (n == sizeof(c)) status_ = fromCache(c);
}

bool PrinterApp::doPoll(AppContext& ctx) {
    PrinterStatus s;
    if (!ctx.printer.pollStatus(s)) return false;

    const bool s_printing = (s.state == PrinterStatus::PrintSt::Printing ||
                             s.state == PrinterStatus::PrintSt::Paused);
    // Once per file: one metadata GET (slicer estimate + byte range + thumbnail).
    bool new_file = false;
    if (s_printing && !s.filename.empty() && s.filename != slicer_est_file_) {
        new_file = true;
        slicer_est_file_ = s.filename;
        slicer_est_s_ = 0; gcode_start_ = gcode_end_ = 0;
        thumb_gray_.clear(); thumb_w_ = thumb_h_ = 0;
        MoonrakerClient::FileMeta meta;
        if (ctx.printer.fetchFileMeta(s.filename, meta)) {
            slicer_est_s_ = meta.estimatedS;
            gcode_start_ = meta.gcodeStart; gcode_end_ = meta.gcodeEnd;
            if (!meta.thumbPng.empty() &&
                !pngDecodeToGray(meta.thumbPng.data(), meta.thumbPng.size(),
                                 thumb_gray_, thumb_w_, thumb_h_, 260, 200)) {
                thumb_gray_.clear(); thumb_w_ = thumb_h_ = 0;
            }
        }
    }
    // File-relative progress (Fluidd-style): raw virtual_sdcard progress counts
    // the start gcode + read-ahead; the metadata byte range bounds the real body.
    if (s_printing && gcode_end_ > gcode_start_ && s.filePosition > 0) {
        s.progress = fileRelativeProgress(s.filePosition, gcode_start_, gcode_end_);
    }

    bool first = !got_first_;
    got_first_ = true;
    Mode before = computeMode();
    bool changed = first || new_file || !sameForDisplay(s, status_);
    status_ = s;
    PrinterCache c = toCache(status_);
    ctx.config.putPrinterCache(&c, sizeof(c));
    Mode after = computeMode();
    // Printing -> Idle (print finished): refresh power + history.
    if (before == Mode::Printing && after == Mode::Idle) {
        refreshPower(ctx); refreshHistory(ctx); changed = true;
    }
    // A real Printing status clears the start override.
    if (after == Mode::Printing) start_override_ = false;
    return changed || before != after;
}

void PrinterApp::refreshPower(AppContext& ctx) {
    PowerState p = ctx.printer.fetchPower();
    if (p != PowerState::Unknown) power_ = p;
}

void PrinterApp::refreshHistory(AppContext& ctx) {
    std::vector<HistoryEntry> h;
    if (ctx.printer.fetchHistory(h, kHistoryShow)) { history_ = h; history_loaded_ = true; }
}

void PrinterApp::tick(AppContext& ctx) {
    if (no_url_ || offline_) return;
    uint32_t now = millis();

    // Expire the start override deadline.
    if (start_override_ && static_cast<int32_t>(now - start_override_until_ms_) >= 0) {
        start_override_ = false;
        render(ctx);
    }
    // Expire the failed banner.
    if (failed_until_ms_ && static_cast<int32_t>(now - failed_until_ms_) >= 0) {
        failed_until_ms_ = 0;
        render(ctx);
    }

    if (confirm_pending_ && static_cast<int32_t>(now - confirm_at_ms_) >= 0) {
        confirm_pending_ = false;
        last_poll_ms_ = now;
        if (computeMode() == Mode::Idle) refreshPower(ctx);
        bool ch = doPoll(ctx);
        if (ch || power_ != power_drawn_) render(ctx);
        return;
    }
    uint32_t interval = (computeMode() == Mode::Printing || start_override_)
                            ? kPollIntervalMs : kIdlePollIntervalMs;
    if (now - last_poll_ms_ >= interval) {
        last_poll_ms_ = now;
        if (doPoll(ctx)) render(ctx);
    }
}

bool PrinterApp::onBack(AppContext& ctx) {
    if (dialog_.active()) { dialog_.close(); render(ctx); return true; }
    return false;   // let the router do hierarchical back (-> launcher)
}

void PrinterApp::noteFailed(AppContext& ctx) {
    failed_until_ms_ = millis() + kFailedBannerMs;
    render(ctx);
}

void PrinterApp::onInput(const InputEvent& e, AppContext& ctx) {
    if (no_url_) return;

    // Modal: swallow ALL input except the two dialog buttons.
    if (dialog_.active()) {
        if (e.kind != InputEvent::Kind::TouchUp) return;
        auto hit = dialog_.hitTest(e.x, e.y);
        if (hit == ConfirmDialog::Hit::Yes) {
            ConfirmKind k = dialog_.kind();
            std::string payload = dialog_.payload();
            dialog_.close();
            runConfirmed(ctx, k, payload);
        } else if (hit == ConfirmDialog::Hit::No) {
            dialog_.close();
            render(ctx);
        }
        return;
    }

    if (computeMode() == Mode::Printing) {
        if (e.kind != InputEvent::Kind::TouchUp) return;
        if (pause_btn_.contains({e.x, e.y})) {
            if (status_.state == PrinterStatus::PrintSt::Paused) {
                // Resume: safe (just continues) — no confirm.
                if (!ctx.printer.ensureWifi()) { offline_ = true; render(ctx); return; }
                offline_ = false;
                if (ctx.printer.resumePrint()) {
                    status_.state = PrinterStatus::PrintSt::Printing;   // optimistic
                    render(ctx);
                    confirm_pending_ = true;
                    confirm_at_ms_ = millis() + kConfirmDelayMs;
                } else {
                    noteFailed(ctx);
                }
            } else {
                dialog_.open(ConfirmKind::PausePrint, tr(Str::printer_confirm_pause));
                render(ctx);
            }
            return;
        }
        if (cancel_btn_.contains({e.x, e.y})) {
            dialog_.open(ConfirmKind::CancelPrint, tr(Str::printer_confirm_cancel));
            render(ctx);
        } else if (light_btn_.contains({e.x, e.y})) {
            onLightTap(ctx);
        }
        return;
    }

    // During the optimistic start-override window the Printing screen is shown
    // but computeMode() is still Idle; ignore idle taps until real status arrives.
    if (start_override_) return;
    // Idle mode.
    if (e.kind == InputEvent::Kind::NavUp)   { scrollBy(-1, ctx); return; }
    if (e.kind == InputEvent::Kind::NavDown) { scrollBy(+1, ctx); return; }
    if (e.kind != InputEvent::Kind::TouchUp) return;
    if (power_btn_.w > 0 && power_btn_.contains({e.x, e.y})) { onPowerTap(ctx); return; }
    if (light_btn_.contains({e.x, e.y})) { onLightTap(ctx); return; }
    if (restart_btn_.w > 0 && restart_btn_.contains({e.x, e.y})) {
        dialog_.open(ConfirmKind::FirmwareRestart, tr(Str::printer_confirm_restart));
        render(ctx); return;
    }
    for (int i = 0; i < 7; ++i) {
        if (jog_[i].w > 0 && jog_[i].contains({e.x, e.y})) { onJogTap(ctx, i); return; }
    }
    if (preheat_btn_.w > 0 && preheat_btn_.contains({e.x, e.y})) {
        if (ctx.printer.ensureWifi()) { offline_ = false; if (!ctx.printer.preheat(preheat_nozzle_, preheat_bed_)) noteFailed(ctx); else { confirm_pending_ = true; confirm_at_ms_ = millis() + kConfirmDelayMs; } }
        else { offline_ = true; render(ctx); }
        return;
    }
    if (cooldown_btn_.w > 0 && cooldown_btn_.contains({e.x, e.y})) {
        if (ctx.printer.ensureWifi()) { offline_ = false; if (!ctx.printer.cooldown()) noteFailed(ctx); else { confirm_pending_ = true; confirm_at_ms_ = millis() + kConfirmDelayMs; } }
        else { offline_ = true; render(ctx); }
        return;
    }
    for (size_t i = 0; i < kHistoryShow && i < history_.size(); ++i) {
        if (hist_rows_[i].w > 0 && hist_rows_[i].contains({e.x, e.y})) {
            std::string nm = history_[i].displayName;
            if (nm.size() > 20) nm = nm.substr(0, 19) + "\xE2\x80\xA6";
            char msg[96];
            std::snprintf(msg, sizeof(msg), tr(Str::printer_confirm_start_fmt), nm.c_str());
            dialog_.open(ConfirmKind::StartPrint, msg, history_[i].filename);
            render(ctx);
            return;
        }
    }
}

void PrinterApp::onPowerTap(AppContext& ctx) {
    if (!ctx.printer.ensureWifi()) { offline_ = true; render(ctx); return; }
    offline_ = false;
    if (power_ == PowerState::On) {
        dialog_.open(ConfirmKind::PowerOff, tr(Str::printer_confirm_poweroff));
        render(ctx);
        return;
    }
    // Powering on is non-destructive — no confirm.
    if (ctx.printer.setPower(true)) {
        power_ = PowerState::On;
        render(ctx);
        confirm_pending_ = true;
        confirm_at_ms_ = millis() + kConfirmDelayMs;
    } else {
        noteFailed(ctx);
    }
}

void PrinterApp::scrollBy(int dir, AppContext& ctx) {
    const int top = kContentTopY + 10;
    const int viewH = kScreenH - top;
    const int contentH = content_bottom_ - top;
    const int span = contentH - viewH;
    if (span <= 0) return;
    const int step = 120;
    int next = scroll_offset_ + dir * step;
    if (next < 0) next = 0;
    if (next > span) next = span;
    if (next == scroll_offset_) return;
    scroll_offset_ = next;
    render(ctx);
}

void PrinterApp::onLightTap(AppContext& ctx) {
    if (!ctx.printer.ensureWifi()) { offline_ = true; render(ctx); return; }
    offline_ = false;
    bool target = !(status_.lightKnown ? status_.lightOn : false);
    if (ctx.printer.setLight(target)) {
        status_.lightOn = target;
        status_.lightKnown = true;
        render(ctx);
        confirm_pending_ = true;
        confirm_at_ms_ = millis() + kConfirmDelayMs;
    }
}

void PrinterApp::runConfirmed(AppContext& ctx, ConfirmKind kind, const std::string& payload) {
    if (!ctx.printer.ensureWifi()) { offline_ = true; render(ctx); return; }
    offline_ = false;
    bool ok = false;
    switch (kind) {
        case ConfirmKind::CancelPrint:     ok = ctx.printer.cancelPrint(); break;
        case ConfirmKind::PowerOff:        ok = ctx.printer.setPower(false); break;
        case ConfirmKind::FirmwareRestart: ok = ctx.printer.firmwareRestart(); break;
        case ConfirmKind::PausePrint:
            ok = ctx.printer.pausePrint();
            if (ok) status_.state = PrinterStatus::PrintSt::Paused;    // optimistic
            break;
        case ConfirmKind::StartPrint:
            ok = ctx.printer.startPrint(payload);
            if (ok) {
                status_.state = PrinterStatus::PrintSt::Printing;   // optimistic
                start_override_ = true;
                start_override_until_ms_ = millis() + kStartOverrideMs;
            }
            break;
        default: break;
    }
    if (!ok) { noteFailed(ctx); return; }
    render(ctx);
    confirm_pending_ = true;                       // re-poll to confirm
    confirm_at_ms_ = millis() + kConfirmDelayMs;
}

void PrinterApp::render(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts;

    if (no_url_) {
        fonts.apply(c, FontFace::Serif, kHeaderFontPx);
        c.setTextColor(15);
        c.drawString(tr(Str::app_printer), kHeaderTextX, kHeaderTextY);
        fonts.apply(c, FontFace::Serif, 24);
        c.drawString(tr(Str::printer_no_url), 30, 220);
        ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
        return;
    }

    Mode m = (start_override_) ? Mode::Printing : computeMode();
    if (m == Mode::Printing) renderPrinting(ctx);
    else                     renderIdle(ctx);

    // Repaint the header band last — scrolled idle content may have painted into it.
    c.fillRect(0, 0, kScreenW, kContentTopY, 0);
    fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.setTextColor(15);
    c.drawString(tr(Str::app_printer), kHeaderTextX, kHeaderTextY);
    if (offline_ || !got_first_) {
        fonts.apply(c, FontFace::Serif, 20);
        c.drawString(tr(offline_ ? Str::weather_offline : Str::boot_loading),
                     kScreenW - 160, kHeaderTextY + 8);
    }

    if (failed_until_ms_) {
        fonts.apply(c, FontFace::Serif, 24);
        c.drawString(tr(Str::printer_action_failed), 30, kScreenH - 40);
    }

    power_drawn_ = power_;
    dialog_.render(c);

    // Layout signature: when unchanged between consecutive Printing renders,
    // only the dynamic stripe needs a push (progress/temps/times), GL16 partial.
    std::string sig;
    if (m == Mode::Printing) {
        bool klippy_bad = status_.klippy != PrinterStatus::Klippy::Ready &&
                          status_.klippy != PrinterStatus::Klippy::Unknown;
        sig = "P|" + status_.filename +
              (status_.state == PrinterStatus::PrintSt::Paused ? "|p" : "|r") +
              (klippy_bad ? "|K" : "") + (thumb_gray_.empty() ? "" : "|T") +
              ((offline_ || !got_first_) ? "|O" : "");
    } else {
        sig = "I";
    }
    bool partial_ok = (m == Mode::Printing) && !dialog_.active() &&
                      failed_until_ms_ == 0 && sig == render_sig_;
    render_sig_ = dialog_.active() ? std::string() : sig;   // dialog forces a Full next time
    if (partial_ok) {
        // Dynamic stripe: progress bar .. elapsed/slicer lines (4-aligned rows).
        // Excludes the state headline and thumbnail (88..214), which only update
        // on Full pushes — keeps the thumbnail from repainting every 3s poll.
        ctx.display.pushRegion({0, 224, kScreenW, 336}, PushMode::Partial);
    } else {
        ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
    }
}

void PrinterApp::renderPrinting(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    Fonts fonts;
    char buf[80];
    int16_t y = kContentTopY + 10;

    bool klippy_bad = status_.klippy != PrinterStatus::Klippy::Ready &&
                      status_.klippy != PrinterStatus::Klippy::Unknown;
    if (klippy_bad) {
        const char* ks = "?";
        switch (status_.klippy) {
            case PrinterStatus::Klippy::Startup:  ks = "startup";  break;
            case PrinterStatus::Klippy::Shutdown: ks = "shutdown"; break;
            case PrinterStatus::Klippy::Error:    ks = "error";    break;
            default: break;
        }
        std::snprintf(buf, sizeof(buf), tr(Str::printer_klippy_fmt), ks);
        fonts.apply(c, FontFace::Serif, 34);
        c.drawString(buf, 30, y); y += 50;
        if (!status_.klippyMessage.empty()) {
            fonts.apply(c, FontFace::Serif, 20);
            c.drawString(status_.klippyMessage.c_str(), 30, y); y += 40;
        }
    } else {
        Str stId;
        switch (status_.state) {
            case PrinterStatus::PrintSt::Printing:  stId = Str::printer_state_printing;  break;
            case PrinterStatus::PrintSt::Paused:    stId = Str::printer_state_paused;    break;
            case PrinterStatus::PrintSt::Complete:  stId = Str::printer_state_complete;  break;
            case PrinterStatus::PrintSt::Cancelled: stId = Str::printer_state_cancelled; break;
            case PrinterStatus::PrintSt::Error:     stId = Str::printer_state_error;     break;
            default:                                stId = Str::printer_state_idle;      break;
        }
        fonts.apply(c, FontFace::Serif, 40);
        c.drawString(tr(stId), 30, y);

        // Thumbnail (if any): top-right, next to the state headline, above the
        // partial-refresh stripe — so the 3s poll never repaints it (no flicker).
        if (!thumb_gray_.empty()) {
            const int th_x = kScreenW - 30 - thumb_w_, th_y = 88;
            c.drawRect(th_x - 2, th_y - 2, thumb_w_ + 4, thumb_h_ + 4, 15);
            const uint8_t* p = thumb_gray_.data();
            for (int yy = 0; yy < thumb_h_; ++yy) {
                for (int xx = 0; xx < thumb_w_; ++xx) {
                    uint8_t ink = static_cast<uint8_t>(15 - (p[yy * thumb_w_ + xx] * 15) / 255);
                    if (ink) c.drawPixel(th_x + xx, th_y + yy, ink);
                }
            }
        }

        y = 228;   // below state headline (90..130) and thumbnail (88..212)
        int pct = static_cast<int>(status_.progress * 100.f + 0.5f);
        const int barX = 30, barW = kScreenW - 60, barH = 30;
        c.drawRect(barX, y, barW, barH, 15);
        int fillW = (barW - 4) * pct / 100;
        if (fillW > 0) c.fillRect(barX + 2, y + 2, fillW, barH - 4, 15);
        fonts.apply(c, FontFace::Serif, 24);
        std::snprintf(buf, sizeof(buf), "%d %%", pct);
        c.drawString(buf, barX, y + barH + 8);
        y += barH + 8 + 44;

        if (!status_.filename.empty()) {
            fonts.apply(c, FontFace::Serif, 22);
            std::string fn = status_.filename;
            if (fn.size() > 40) fn = fn.substr(0, 39) + "\xE2\x80\xA6";
            c.drawString(fn.c_str(), 30, y); y += 44;
        }

        // temps on one line, then layer
        fonts.apply(c, FontFace::Serif, 26);
        std::snprintf(buf, sizeof(buf), "%s %d\xE2\x86\x92%d \xC2\xB7 %s %d\xE2\x86\x92%d",
                      tr(Str::printer_hotend), ireg(status_.hotendTemp), ireg(status_.hotendTarget),
                      tr(Str::printer_bed), ireg(status_.bedTemp), ireg(status_.bedTarget));
        c.drawString(buf, 30, y); y += 40;
        if (status_.currentLayer >= 0 && status_.totalLayer > 0) {
            std::snprintf(buf, sizeof(buf), tr(Str::printer_layer_fmt),
                          status_.currentLayer, status_.totalLayer);
            c.drawString(buf, 30, y); y += 40;
        }

        char el[16];
        fmtHM(status_.printDurationS, el, sizeof(el));
        uint32_t rem;
        if (slicer_est_s_ > status_.printDurationS) rem = slicer_est_s_ - status_.printDurationS;
        else rem = etaRemainingSec(status_.printDurationS, status_.progress);
        fonts.apply(c, FontFace::Serif, 24);
        if (rem > 0) {
            char et[16];
            fmtHM(rem, et, sizeof(et));
            std::snprintf(buf, sizeof(buf), "%s %s \xC2\xB7 %s ~%s",
                          tr(Str::printer_elapsed), el, tr(Str::printer_eta), et);
        } else {
            std::snprintf(buf, sizeof(buf), "%s %s", tr(Str::printer_elapsed), el);
        }
        c.drawString(buf, 30, y); y += 44;
        if (slicer_est_s_ > 0) {
            char st[16];
            fmtHM(slicer_est_s_, st, sizeof(st));
            std::snprintf(buf, sizeof(buf), "%s ~%s", tr(Str::printer_slicer), st);
            c.drawString(buf, 30, y); y += 44;
        }
    }

    // Bottom row: light toggle (icon) + Pause/Resume + Cancel.
    const int bh = 110, gap = 20;
    const int lw = bh;                                   // square icon button
    const int tw = (kScreenW - 60 - lw - gap * 2) / 2;   // pause + cancel split the rest
    const int bx = 30, by = kScreenH - bh - 30;
    light_btn_  = {static_cast<int16_t>(bx), static_cast<int16_t>(by),
                   static_cast<int16_t>(lw), static_cast<int16_t>(bh)};
    pause_btn_  = {static_cast<int16_t>(bx + lw + gap), static_cast<int16_t>(by),
                   static_cast<int16_t>(tw), static_cast<int16_t>(bh)};
    cancel_btn_ = {static_cast<int16_t>(bx + lw + gap + tw + gap), static_cast<int16_t>(by),
                   static_cast<int16_t>(tw), static_cast<int16_t>(bh)};
    c.drawRect(light_btn_.x, light_btn_.y, light_btn_.w, light_btn_.h, 15);
    c.drawRect(pause_btn_.x, pause_btn_.y, pause_btn_.w, pause_btn_.h, 15);
    c.drawRect(cancel_btn_.x, cancel_btn_.y, cancel_btn_.w, cancel_btn_.h, 15);
    const int isz = bh - 28;
    IconKit::lightbulb(c, light_btn_.x + (lw - isz) / 2, light_btn_.y + (bh - isz) / 2,
                       isz, status_.lightKnown && status_.lightOn);
    const bool paused = status_.state == PrinterStatus::PrintSt::Paused;
    fonts.apply(c, FontFace::Serif, 28);
    c.setTextDatum(MC_DATUM);
    c.drawString(tr(paused ? Str::printer_resume : Str::printer_pause),
                 pause_btn_.x + tw / 2, pause_btn_.y + bh / 2);
    c.drawString(tr(Str::printer_cancel), cancel_btn_.x + tw / 2, cancel_btn_.y + bh / 2);
    c.setTextDatum(TL_DATUM);
}

void PrinterApp::renderIdle(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    int y = (kContentTopY + 10) - scroll_offset_;

    y = drawStatusBand(c, y);
    y = drawMoveSection(c, y);
    y = drawPreheatSection(c, y);
    y = drawHistorySection(c, y);

    content_bottom_ = y + scroll_offset_;

    // Scrollbar (right edge) when content overflows.
    const int top = kContentTopY + 10;
    const int viewH = kScreenH - top;
    const int contentH = content_bottom_ - top;
    if (contentH > viewH) {
        const int trackX = kScreenW - 10, trackY = top, trackH = viewH;
        c.drawRect(trackX, trackY, 6, trackH, 15);
        int span = contentH - viewH;
        int knobH = trackH * viewH / contentH;
        int knobY = trackY + (span > 0 ? scroll_offset_ : 0) * (trackH - knobH) / (span > 0 ? span : 1);
        c.fillRect(trackX, knobY, 6, knobH, 15);
    }

    cancel_btn_ = {0, 0, 0, 0};
    pause_btn_ = {0, 0, 0, 0};
}

int PrinterApp::drawStatusBand(M5EPD_Canvas& c, int y) {
    Fonts fonts;
    char buf[80];
    const int bh = 90, gap = 14;

    // Right-aligned icon toggles: [power][light].
    int x = kScreenW - 30 - bh;                    // light is rightmost
    light_btn_ = {static_cast<int16_t>(x), static_cast<int16_t>(y),
                  static_cast<int16_t>(bh), static_cast<int16_t>(bh)};
    c.drawRect(x, y, bh, bh, 15);
    const int isz = bh - 22;
    IconKit::lightbulb(c, x + (bh - isz) / 2, y + (bh - isz) / 2, isz,
                       status_.lightKnown && status_.lightOn);
    if (has_power_dev_) {
        x -= bh + gap;
        power_btn_ = {static_cast<int16_t>(x), static_cast<int16_t>(y),
                      static_cast<int16_t>(bh), static_cast<int16_t>(bh)};
        c.drawRect(x, y, bh, bh, 15);
        IconKit::power(c, x + (bh - isz) / 2, y + (bh - isz) / 2, isz,
                       power_ == PowerState::On);
    } else {
        power_btn_ = {0, 0, 0, 0};
    }

    // Left column: state headline + temps line.
    const bool ready = status_.klippy == PrinterStatus::Klippy::Ready;
    const char* head;
    if (has_power_dev_ && power_ == PowerState::Off) {
        head = tr(Str::printer_powered_off);
    } else if (!ready) {
        head = (status_.klippy == PrinterStatus::Klippy::Startup)
                   ? tr(Str::printer_klipper_starting)
                   : tr(Str::printer_not_ready);
    } else {
        Str sid;
        switch (status_.state) {
            case PrinterStatus::PrintSt::Complete:  sid = Str::printer_state_complete;  break;
            case PrinterStatus::PrintSt::Cancelled: sid = Str::printer_state_cancelled; break;
            case PrinterStatus::PrintSt::Error:     sid = Str::printer_state_error;     break;
            default:                                sid = Str::printer_state_idle;      break;
        }
        head = tr(sid);
    }
    fonts.apply(c, FontFace::Serif, 28);
    c.drawString(head, 30, y + 4);
    fonts.apply(c, FontFace::Serif, 20);
    if (ready) {
        std::snprintf(buf, sizeof(buf), "%s %d\xE2\x86\x92%d", tr(Str::printer_hotend),
                      ireg(status_.hotendTemp), ireg(status_.hotendTarget));
        c.drawString(buf, 30, y + 42);
        std::snprintf(buf, sizeof(buf), "%s %d\xE2\x86\x92%d", tr(Str::printer_bed),
                      ireg(status_.bedTemp), ireg(status_.bedTarget));
        c.drawString(buf, 30, y + 66);
    } else {
        std::snprintf(buf, sizeof(buf), "%s \xE2\x80\x94", tr(Str::printer_hotend));
        c.drawString(buf, 30, y + 42);
        std::snprintf(buf, sizeof(buf), "%s \xE2\x80\x94", tr(Str::printer_bed));
        c.drawString(buf, 30, y + 66);
    }
    return y + bh + 24;
}

int PrinterApp::drawMoveSection(M5EPD_Canvas& c, int y) {
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, 26);
    c.drawString(tr(Str::printer_move), 30, y); y += 40;

    const bool ready = status_.klippy == PrinterStatus::Klippy::Ready;

    if (!ready) {
        // Gating hint by cause.
        Str hint = Str::printer_not_ready;
        if (has_power_dev_ && power_ == PowerState::Off) hint = Str::printer_power_locked;
        else if (status_.klippy == PrinterStatus::Klippy::Startup) hint = Str::printer_klipper_starting;
        fonts.apply(c, FontFace::Serif, 24);
        c.drawString(tr(hint), 30, y); y += 40;
        // Offer firmware restart when powered but shut down / errored.
        bool offer_restart = (status_.klippy == PrinterStatus::Klippy::Shutdown ||
                              status_.klippy == PrinterStatus::Klippy::Error) &&
                             (!has_power_dev_ || power_ != PowerState::Off);
        if (offer_restart) {
            const int bw = 320, bh = 84;
            restart_btn_ = {30, static_cast<int16_t>(y), bw, bh};
            c.drawRect(30, y, bw, bh, 15);
            c.drawString(tr(Str::printer_firmware_restart), 46, y + 30);
            y += bh + 16;
        } else {
            restart_btn_ = {0, 0, 0, 0};
        }
        // Disable jog rects.
        for (auto& r : jog_) r = {0, 0, 0, 0};
        return y;
    }
    restart_btn_ = {0, 0, 0, 0};

    // Cross-pad geometry. Cell 96x84, centered-ish on the left; Z column on the right.
    const int cw = 96, ch = 84, gap = 12;
    const int cx = 30 + cw + gap;          // center column x (for Y+/Home/Y-)
    const int row0 = y, row1 = y + ch + gap, row2 = y + 2 * (ch + gap);
    // indices: 0:X- 1:X+ 2:Y- 3:Y+ 4:Z- 5:Z+ 6:Home
    jog_[3] = {static_cast<int16_t>(cx),            static_cast<int16_t>(row0), cw, ch};   // Y+
    jog_[0] = {static_cast<int16_t>(30),            static_cast<int16_t>(row1), cw, ch};   // X-
    jog_[6] = {static_cast<int16_t>(cx),            static_cast<int16_t>(row1), cw, ch};   // Home
    jog_[1] = {static_cast<int16_t>(cx + cw + gap), static_cast<int16_t>(row1), cw, ch};   // X+
    jog_[2] = {static_cast<int16_t>(cx),            static_cast<int16_t>(row2), cw, ch};   // Y-
    const int zx = cx + 2 * (cw + gap) + gap;
    jog_[5] = {static_cast<int16_t>(zx),            static_cast<int16_t>(row0), cw, ch};   // Z+
    jog_[4] = {static_cast<int16_t>(zx),            static_cast<int16_t>(row2), cw, ch};   // Z-

    fonts.apply(c, FontFace::Serif, 30);
    bool hx = axisHomed(status_.homedAxes, 'x');
    bool hy = axisHomed(status_.homedAxes, 'y');
    bool hz = axisHomed(status_.homedAxes, 'z');
    jogBtn(c, jog_[3], "Y+", hy);
    jogBtn(c, jog_[0], "X-", hx);
    jogBtn(c, jog_[6], "\xE2\x8C\x82", true);   // ⌂ home always (ready)
    jogBtn(c, jog_[1], "X+", hx);
    jogBtn(c, jog_[2], "Y-", hy);
    jogBtn(c, jog_[5], "Z+", hz);
    jogBtn(c, jog_[4], "Z-", hz);

    // Disabled axes are drawn greyed for feedback, but their hit-rects are
    // zeroed so the `.w > 0` guard in onInput blocks taps on un-homed axes
    // (Home rect, index 6, always stays live when ready).
    if (!hx) { jog_[0] = {0, 0, 0, 0}; jog_[1] = {0, 0, 0, 0}; }
    if (!hy) { jog_[2] = {0, 0, 0, 0}; jog_[3] = {0, 0, 0, 0}; }
    if (!hz) { jog_[4] = {0, 0, 0, 0}; jog_[5] = {0, 0, 0, 0}; }

    return row2 + ch + 24;
}

int PrinterApp::drawPreheatSection(M5EPD_Canvas& c, int y) {
    Fonts fonts;
    const bool ready = status_.klippy == PrinterStatus::Klippy::Ready;
    if (!ready) {
        preheat_btn_ = {0, 0, 0, 0};
        cooldown_btn_ = {0, 0, 0, 0};
        return y;
    }
    const int bh = 84, gap = 20, bw = (kScreenW - 60 - gap) / 2;
    preheat_btn_  = {30, static_cast<int16_t>(y), bw, bh};
    cooldown_btn_ = {static_cast<int16_t>(30 + bw + gap), static_cast<int16_t>(y), bw, bh};
    c.drawRect(preheat_btn_.x, preheat_btn_.y, bw, bh, 15);
    c.drawRect(cooldown_btn_.x, cooldown_btn_.y, bw, bh, 15);
    char lbl[32];
    std::snprintf(lbl, sizeof(lbl), tr(Str::printer_heat_fmt), preheat_nozzle_, preheat_bed_);
    fonts.apply(c, FontFace::Serif, 24);
    c.setTextDatum(MC_DATUM);
    c.drawString(lbl, preheat_btn_.x + preheat_btn_.w / 2, preheat_btn_.y + preheat_btn_.h / 2);
    c.drawString(tr(Str::printer_cooldown), cooldown_btn_.x + cooldown_btn_.w / 2, cooldown_btn_.y + cooldown_btn_.h / 2);
    c.setTextDatum(TL_DATUM);
    return y + bh + 24;
}

int PrinterApp::drawHistorySection(M5EPD_Canvas& c, int y) {
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, 26);
    c.drawString(tr(Str::printer_history), 30, y); y += 42;

    for (auto& r : hist_rows_) r = {0, 0, 0, 0};

    if (history_.empty()) {
        fonts.apply(c, FontFace::Serif, 22);
        c.drawString(tr(history_loaded_ ? Str::printer_history_empty : Str::boot_loading), 30, y);
        return y + 40;
    }

    const int rh = 70;
    size_t n = history_.size();
    if (n > kHistoryShow) n = kHistoryShow;
    for (size_t i = 0; i < n; ++i) {
        hist_rows_[i] = {30, static_cast<int16_t>(y),
                         static_cast<int16_t>(kScreenW - 60), rh};
        c.drawRect(hist_rows_[i].x, hist_rows_[i].y, hist_rows_[i].w, hist_rows_[i].h, 15);
        std::string nm = history_[i].displayName;
        if (nm.size() > 40) nm = nm.substr(0, 39) + "\xE2\x80\xA6";
        fonts.apply(c, FontFace::Serif, 20);
        c.drawString(("\xE2\x96\xB8 " + nm).c_str(), hist_rows_[i].x + 14, hist_rows_[i].y + 12);
        char dt[20];
        formatPrintTime(history_[i].lastPrint, tz_offset_, dt, sizeof(dt));
        fonts.apply(c, FontFace::Serif, 18);
        c.drawString(dt, hist_rows_[i].x + 40, hist_rows_[i].y + 42);
        y += rh + 10;
    }
    return y + 14;
}

void PrinterApp::onJogTap(AppContext& ctx, int idx) {
    if (!ctx.printer.ensureWifi()) { offline_ = true; render(ctx); return; }
    offline_ = false;
    bool ok = false;
    switch (idx) {
        case 0: ok = ctx.printer.jog('X', -kJogStepMm, kJogFeedXY); break;
        case 1: ok = ctx.printer.jog('X', +kJogStepMm, kJogFeedXY); break;
        case 2: ok = ctx.printer.jog('Y', -kJogStepMm, kJogFeedXY); break;
        case 3: ok = ctx.printer.jog('Y', +kJogStepMm, kJogFeedXY); break;
        case 4: ok = ctx.printer.jog('Z', -kJogStepMm, kJogFeedZ);  break;
        case 5: ok = ctx.printer.jog('Z', +kJogStepMm, kJogFeedZ);  break;
        case 6: ok = ctx.printer.home(); break;
    }
    if (!ok) { noteFailed(ctx); return; }
    confirm_pending_ = true;             // re-poll to refresh homed_axes/temps
    confirm_at_ms_ = millis() + kConfirmDelayMs;
}

} // namespace paperos
