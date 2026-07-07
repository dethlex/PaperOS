#pragma once
#include <M5EPD.h>
#include <string>
#include <cstdint>
#include "framework/ui/Geometry.h"

namespace paperos {

enum class ConfirmKind : uint8_t { None, CancelPrint, StartPrint, PowerOff, FirmwareRestart, PausePrint };

// Modal yes/no overlay. State is held here; PrinterApp drives open/close and acts
// on the confirmed kind/payload. Drawn at fixed screen coordinates.
class ConfirmDialog {
public:
    enum class Hit { None, Yes, No };

    void open(ConfirmKind kind, const std::string& message, const std::string& payload = "");
    void close() { kind_ = ConfirmKind::None; }
    bool active() const { return kind_ != ConfirmKind::None; }
    ConfirmKind kind() const { return kind_; }
    const std::string& payload() const { return payload_; }

    void render(M5EPD_Canvas& c) const;   // call after drawing the background
    Hit hitTest(int x, int y) const;

private:
    ConfirmKind kind_ = ConfirmKind::None;
    std::string message_;
    std::string payload_;

    // Fixed layout (centered box). Buttons sized > 60x80 for touch.
    static constexpr int kBoxW = 440, kBoxH = 300;
    static constexpr int kBoxX = (kScreenW - kBoxW) / 2;
    static constexpr int kBoxY = (kScreenH - kBoxH) / 2;
    static constexpr int kBtnW = 180, kBtnH = 90;
    static constexpr int kBtnY = kBoxY + kBoxH - kBtnH - 24;
    static constexpr int kNoX  = kBoxX + 24;
    static constexpr int kYesX = kBoxX + kBoxW - kBtnW - 24;
};

} // namespace paperos
