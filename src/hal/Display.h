#pragma once
#include <M5EPD.h>
#include "framework/ui/Geometry.h"

namespace paperos {

enum class PushMode {
    Partial,  // GL16 — fast, no flicker
    Full,     // GC16 — full refresh with inversion (anti-ghost)
    Quick     // A2 — fast 2-level, for menus
};

class Display {
public:
    // Initialize IT8951 and create main canvas in PSRAM.
    // Idempotent — may be called after deep-sleep wake.
    //
    // `full_init=true` (cold boot, user wake): also runs M5.EPD.Clear(true)
    // which whites out the panel via an INIT waveform. Necessary on first
    // boot to bring IT8951 SRAM and physical panel into a known state.
    //
    // `full_init=false` (Timer wake for minute tick): skip the Clear so
    // the panel keeps its prior image (e-ink retention). Required for
    // partial updates after deep sleep — otherwise the rest of the screen
    // is wiped to white and only the partial rect re-appears.
    void begin(bool full_init = true);

    // Cut the IT8951 rail (GPIO23) powering the controller and TPS65185 PMIC —
    // the dominant idle consumer during light sleep. Also cuts EXT/Grove boost
    // (GPIO5). Sets initialized_ = false. The GPIO2 main-power latch is untouched,
    // so the board stays alive on battery; the e-ink panel physically retains its
    // last frame while powered off.
    void powerDown();

    // Bring the IT8951 back up after powerDown() (GPIO23 power cycle fully resets
    // the controller: registers, image RAM, waveform, VCOM). Mirrors the EPD
    // portion of M5.begin(): re-enable GPIO23/GPIO5, settle, re-run the driver
    // init (safe to re-call — unlike M5.begin() itself), restore rotation.
    // The PSRAM canvas survives in RAM (main rail), so createCanvas() is a
    // no-op early-return. No-op if already initialized. Settle time is kEpdWakeSettleMs
    // (HW-tunable) — call from the sleep/wake path, never from tick().
    void wake();

    // Get the main full-screen canvas. Lives in PSRAM.
    M5EPD_Canvas& canvas() { return canvas_; }

    // Request push of the main canvas. Display will choose actual refresh
    // mode based on PushMode hint and internal counter (forces Full every
    // kFullRefreshEvery partial pushes to clear ghosting).
    //
    // IMPORTANT: the `r` rectangle is currently a HINT ONLY — this impl
    // always pushes the whole canvas at origin (0, 0). M5EPD_Canvas's
    // pushCanvas(x, y, mode) does NOT clip-push a region; it relocates the
    // entire canvas to (x, y) on the panel, which causes duplication /
    // off-screen content if x or y is nonzero. So we ignore r.x/r.y and
    // refresh the whole screen. To refresh only a sub-rectangle (e.g. a
    // clock overlay), build a smaller M5EPD_Canvas and use pushSubCanvas()
    // instead — both SPI bandwidth and e-ink waveform area scale with the
    // sub-canvas size. (A future true-partial pushRegion would copy the
    // r-rectangle out of canvas_ into a temp buffer and call
    // WritePartGram4bpp + UpdateArea like pushSubCanvas does.)
    void pushRegion(Rect r, PushMode mode);

    // Upload a small canvas to the panel at (x, y) and refresh only that
    // rectangle. Uses WritePartGram4bpp + UpdateArea under the hood.
    //
    // IT8951 requires x, y, and the canvas width all multiples of 4 (4bpp
    // packing). Caller is responsible for that alignment.
    void pushSubCanvas(M5EPD_Canvas& sub, int x, int y, PushMode mode);

    // Force-push the full screen with GC16 (used after wallpaper change).
    void pushFullClear();

    // Block until the IT8951 finishes the currently-running waveform.
    // pushCanvas/UpdateArea only kicks the refresh off — without flushing
    // before powerDown() the e-ink can freeze mid-update.
    void flush();

    // True if last push was Full (caller may use for analytics/logs).
    bool lastWasFull() const { return last_full_; }

    static constexpr uint16_t kFullRefreshEvery = 20;

    // Rail-settle wait between enableEPDPower() and re-running EPD.begin() in
    // wake(). M5.begin() uses 1000 ms for a cold boot of the whole board; for
    // the per-minute re-wake of an already-configured rail 250 ms proved
    // sufficient on HW (2026-07-06 power refactor, smoke §11.5). If wake logs
    // stop showing "IT8951 re-init ok" or boxes render garbage, raise this.
    static constexpr uint32_t kEpdWakeSettleMs = 250;

private:
    M5EPD_Canvas canvas_{&M5.EPD};
    uint16_t partial_count_ = 0;
    bool last_full_ = false;
    bool initialized_ = false;
};

} // namespace paperos
