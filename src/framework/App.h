#pragma once
#include <stdint.h>
#include "framework/ui/Geometry.h"

class M5EPD_Canvas;            // forward decl in global namespace

namespace paperos {

class Display;
class ConfigStore;
class Storage;
class Power;
class BookIndex;
class HAClient;
class WeatherService;
class Sd;
class Wifi;
class Rtc;
class Sht30;
class Battery;
class WebDavServer;

struct InputEvent {
    enum class Kind : uint8_t {
        TouchDown, TouchUp, TouchMove,
        ButtonDown, ButtonUp,   // legacy — no app listens to these anymore
        NavUp, NavDown,         // G37 / G39 — semantics are app-dependent
        Idle, Wake
    };
    Kind kind;
    int16_t x = 0, y = 0;
    uint8_t button = 0;    // 0=Up(G39), 1=Mid(G38), 2=Down(G37 — captured by switcher)
    uint32_t now_ms = 0;
};

class AppSwitcher;

struct AppContext {
    Display& display;
    ConfigStore& config;
    Storage& storage;
    Power& power;
    Sd& sd;
    Wifi& wifi;
    Rtc& rtc;
    BookIndex& books;
    HAClient& ha;
    AppSwitcher& switcher;
    WeatherService& weather;
    Sht30& sht30;
    Battery& battery;
    WebDavServer& webdav;
};

class IApp {
public:
    virtual ~IApp() = default;
    virtual void enter(AppContext&) {}
    virtual void leave(AppContext&) {}
    virtual void tick(AppContext&)  {}
    virtual void onInput(const InputEvent&, AppContext&) {}
    // Hierarchical "back" (G38 short-press). Return true if the app moved up
    // one level and consumed the event. Return false (default) if the app is
    // already at its top level — InputRouter then goes to Launcher, or to the
    // lock screen (screensaver) if the app already is Launcher.
    virtual bool onBack(AppContext&) { return false; }
    // If true, InputRouter idle does NOT go to the screensaver (the timer just
    // resets). For apps with background activity but no input (file server).
    virtual bool keepAwake() const { return false; }
    virtual const char* id() const = 0;
    virtual const char* title() const = 0;
    // Optional: render a per-app widget inside a launcher card.
    // Default implementation does nothing — Launcher draws icon+title.
    virtual void renderLauncherWidget(M5EPD_Canvas&, Rect) {}
};

} // namespace paperos
