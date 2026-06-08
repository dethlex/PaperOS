#include "Display.h"
#include "util/Logger.h"

namespace paperos {

void Display::begin(bool full_init) {
    if (initialized_) return;
    M5.EPD.SetRotation(90);                  // portrait 540 x 960
    if (full_init) {
        M5.EPD.Clear(true);                  // INIT waveform — wipes panel to white
    }
    canvas_.createCanvas(kScreenW, kScreenH);
    initialized_ = true;
    LOG_INFO("display", "init ok %dx%d (full_init=%d)", kScreenW, kScreenH,
             static_cast<int>(full_init));
}

void Display::powerDown() {
    // Cut the ACTUAL e-paper rail: GPIO23 (M5EPD_EPD_PWR_EN_PIN) powers the IT8951
    // controller + TPS65185 EPD PMIC — the dominant idle consumer. The previous
    // code cut only GPIO5 (disableEXTPower = EXT/Grove boost), which does NOT power
    // the IT8951, so the controller kept running in SYS_RUN through every light
    // sleep (the lock-screen battery-drain bug). We drop both rails; the GPIO2
    // main-power latch stays HIGH so the board survives on battery and the e-ink
    // panel physically retains its frame without power.
    M5.disableEPDPower();                    // GPIO23 — actually powers off the IT8951
    M5.disableEXTPower();                    // GPIO5  — EXT/Grove boost (secondary)
    initialized_ = false;
}

void Display::wake() {
    if (initialized_) return;                // already up
    M5.enableEPDPower();                      // GPIO23 HIGH — restore IT8951/EPD rail
    M5.enableEXTPower();                      // GPIO5 HIGH — restore EXT/Grove boost
    delay(1000);                              // rail settle (matches M5.begin before EPD.begin)
    // Re-run the IT8951 driver init. Safe to re-call (no guard, SPI.begin twice
    // is OK); reloads SYS_RUN + VCOM. _tar_memaddr survives in RAM, so no
    // GetSysInfo needed. M5.begin() itself is NOT re-entrant — do not call it.
    M5.EPD.begin(M5EPD_SCK_PIN, M5EPD_MOSI_PIN, M5EPD_MISO_PIN,
                 M5EPD_CS_PIN, M5EPD_BUSY_PIN);
    M5.EPD.SetRotation(90);                   // portrait 540 x 960 (as in begin())
    canvas_.createCanvas(kScreenW, kScreenH); // early-returns if already created — no PSRAM leak
    initialized_ = true;
    LOG_INFO("display", "wake: IT8951 re-init ok");
}

void Display::flush() {
    M5.EPD.CheckAFSR();
}

void Display::pushRegion(Rect r, PushMode mode) {
    bool force_full = (mode == PushMode::Full) ||
                      (partial_count_ + 1 >= kFullRefreshEvery);
    m5epd_update_mode_t m;
    if (force_full)                   m = UPDATE_MODE_GC16;
    else if (mode == PushMode::Quick) m = UPDATE_MODE_A2;
    else                              m = UPDATE_MODE_GL16;

    // `canvas_.pushCanvas(x, y, mode)` does NOT clip-push the canvas to a
    // sub-rectangle — it relocates the WHOLE canvas to (x, y) on the panel,
    // so any nonzero (x, y) shifts the image and produces a duplicated/sliced
    // view. To genuinely refresh only a stripe of the panel, we need
    // WritePartGram4bpp + UpdateArea with a buffer that contains JUST that
    // stripe of pixel data.
    //
    // Lucky case: when r.x == 0 and r.w == kScreenW, the stripe is full-width
    // rows of the main canvas, and the 4bpp packed framebuffer is already in
    // exactly that layout — no copy needed, we just point at canvas[r.y * stride].
    // This is what Keyboard and Reader-style scrolling want.
    //
    // Otherwise (arbitrary rectangle): the canvas's row stride mismatches the
    // partial buffer's stride, so we can't pass canvas pixels directly. Rather
    // than build a temporary copy, we fall back to a full-canvas push at
    // (0, 0) — slower, but visually correct. Callers that really need true
    // arbitrary-rectangle partial should use pushSubCanvas() with their own
    // sub-canvas instead.
    const bool full_width = (r.x == 0 && r.w == kScreenW);
    const bool in_bounds  = (r.y >= 0 && r.h > 0 && r.y + r.h <= kScreenH);
    if (force_full || !full_width || !in_bounds) {
        canvas_.pushCanvas(0, 0, m);
    } else {
        const uint8_t* fb = static_cast<const uint8_t*>(canvas_.frameBuffer());
        const size_t stride = static_cast<size_t>(kScreenW) / 2;   // 4bpp packed
        const uint8_t* slice = fb + static_cast<size_t>(r.y) * stride;
        M5.EPD.WritePartGram4bpp(0, static_cast<uint16_t>(r.y),
                                 static_cast<uint16_t>(kScreenW),
                                 static_cast<uint16_t>(r.h), slice);
        M5.EPD.UpdateArea(0, static_cast<uint16_t>(r.y),
                          static_cast<uint16_t>(kScreenW),
                          static_cast<uint16_t>(r.h), m);
    }
    last_full_ = force_full;
    if (force_full) partial_count_ = 0;
    else            partial_count_++;
}

void Display::pushSubCanvas(M5EPD_Canvas& sub, int x, int y, PushMode mode) {
    bool force_full = (mode == PushMode::Full) ||
                      (partial_count_ + 1 >= kFullRefreshEvery);
    m5epd_update_mode_t m;
    if (force_full)                       m = UPDATE_MODE_GC16;
    else if (mode == PushMode::Quick)     m = UPDATE_MODE_A2;
    else                                  m = UPDATE_MODE_GL16;
    const uint16_t w = static_cast<uint16_t>(sub.width());
    const uint16_t h = static_cast<uint16_t>(sub.height());
    M5.EPD.WritePartGram4bpp(static_cast<uint16_t>(x), static_cast<uint16_t>(y),
                             w, h,
                             reinterpret_cast<const uint8_t*>(sub.frameBuffer()));
    M5.EPD.UpdateArea(static_cast<uint16_t>(x), static_cast<uint16_t>(y), w, h, m);
    last_full_ = force_full;
    if (force_full) partial_count_ = 0;
    else            partial_count_++;
}

void Display::pushFullClear() {
    canvas_.pushCanvas(0, 0, UPDATE_MODE_GC16);
    partial_count_ = 0;
    last_full_ = true;
}

} // namespace paperos
