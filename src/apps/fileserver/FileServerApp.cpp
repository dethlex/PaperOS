#include "FileServerApp.h"
#include "hal/Display.h"
#include "hal/Sd.h"
#include "hal/Wifi.h"
#include "services/ConfigStore.h"
#include "services/WebDavServer.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/Geometry.h"
#include "util/Logger.h"
#include "i18n/Strings.h"
#include <M5EPD.h>
#include <string>

namespace paperos {

void FileServerApp::drawMessage(AppContext& ctx, const char* l1, const char* l2) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, 32);
    c.setTextColor(15);
    c.drawString(l1, 40, kScreenH / 2 - 60);
    if (l2) {
        fonts.apply(c, FontFace::Serif, 24);
        c.drawString(l2, 40, kScreenH / 2);
    }
    ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
}

void FileServerApp::drawStatus(AppContext& ctx, const char* url) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts;
    c.setTextColor(15);
    fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.drawString(tr(Str::app_fileserver), kHeaderTextX, kHeaderTextY);
    fonts.apply(c, FontFace::Serif, 26);
    c.drawString(tr(Str::fs_open_on_computer), 40, 220);
    fonts.apply(c, FontFace::Serif, 34);
    c.drawString(url, 40, 280);
    fonts.apply(c, FontFace::Serif, 24);
    c.drawString(tr(Str::fs_finder_hint), 40, 380);
    c.drawString(tr(Str::fs_root), 40, 430);
    c.drawString(tr(Str::fs_back_stop), 40, 520);
    ctx.display.pushRegion({0, 0, kScreenW, kScreenH}, PushMode::Full);
}

void FileServerApp::enter(AppContext& ctx) {
    if (!ctx.sd.present()) {
        // Without a card the server is pointless: the /paperos root is missing, everything 404s.
        drawMessage(ctx, tr(Str::common_no_sd), tr(Str::fs_insert_card));
        return;
    }
    if (ctx.config.wifiSsid().empty()) {
        drawMessage(ctx, tr(Str::fs_wifi_off), tr(Str::fs_wifi_settings));
        return;
    }
    drawMessage(ctx, tr(Str::fs_connecting), nullptr);
    if (ctx.wifi.connect(ctx.config.wifiSsid(), ctx.config.wifiPass(), 8000) != WifiResult::Ok) {
        drawMessage(ctx, tr(Str::fs_connect_fail), tr(Str::fs_check_wifi));
        return;
    }
    // Draw the status BEFORE starting the server — this is the last EPD push (see
    // spec §4.1: SD and EPD share SPI, so we can't push EPD while the httpd task may
    // be writing SD).
    std::string url = "http://" + ctx.wifi.ipString() + "/";
    drawStatus(ctx, url.c_str());

    if (!ctx.webdav.start(80)) {
        ctx.wifi.disconnect();                 // server didn't start → EPD push is safe
        drawMessage(ctx, tr(Str::fs_server_fail), nullptr);
        return;
    }
    LOG_INFO("fileserver", "serving at %s", url.c_str());
}

void FileServerApp::teardown(AppContext& ctx) {
    if (ctx.webdav.running()) ctx.webdav.stop();   // stop server before any subsequent EPD push
    ctx.wifi.disconnect();
}

bool FileServerApp::onBack(AppContext& ctx) {
    teardown(ctx);
    return false;   // top level → InputRouter goes to the launcher (which renders after teardown)
}

void FileServerApp::leave(AppContext& ctx) {
    teardown(ctx);  // in case of an alternate exit path; idempotent
}

} // namespace paperos
