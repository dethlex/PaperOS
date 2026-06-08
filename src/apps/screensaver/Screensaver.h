#pragma once
#include "framework/App.h"
#include "i18n/Strings.h"

namespace paperos {

class Screensaver : public IApp {
public:
    void enter(AppContext& ctx) override;
    void onInput(const InputEvent& e, AppContext& ctx) override;
    const char* id() const override { return "screensaver"; }
    const char* title() const override { return tr(Str::app_screensaver); }

    // Called from main.cpp at Timer-wake path (minimal boot).
    // Re-renders the current wallpaper+clock without re-decoding the photo.
    static void renderMinuteTick(AppContext& ctx);

    // Renders full screensaver (decodes new photo if needed, writes cache/bg.bin, full refresh).
    static void renderFull(AppContext& ctx, bool rotate_photo);

private:
    static void renderBackground(AppContext& ctx);   // pushes bg.bin to canvas
    static void renderClockOverlay(AppContext& ctx); // draws HH:MM on top
};

} // namespace paperos
