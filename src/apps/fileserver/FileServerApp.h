#pragma once
#include "framework/App.h"
#include "i18n/Strings.h"

namespace paperos {

class FileServerApp : public IApp {
public:
    void enter(AppContext&) override;
    void leave(AppContext&) override;
    void tick(AppContext&) override {}        // server serves in its own task; don't touch EPD
    bool onBack(AppContext&) override;
    bool keepAwake() const override { return true; }
    const char* id() const override { return "fileserver"; }
    // Short name for the launcher tile ("Files"); the in-app header draws the
    // full "File Server" (app_fileserver) directly in render().
    const char* title() const override { return tr(Str::app_files); }

private:
    void teardown(AppContext&);                       // stop server + WiFi off (idempotent)
    void drawMessage(AppContext&, const char* l1, const char* l2);
    void drawStatus(AppContext&, const char* url);
};

} // namespace paperos
