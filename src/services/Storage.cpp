// Storage is the one place that legitimately mixes the Sd HAL guard (`present()`)
// with raw `SD.*` calls (mkdir/exists/open/remove/rename). The Sd wrapper does not
// expose those ops because no other class needs them, and adding wrappers solely
// for Storage would add files without benefit. If a second class ever needs
// these ops, promote them to Sd.
#include "Storage.h"
#include "hal/Sd.h"
#include "util/Logger.h"
#include <SD.h>
#include <stdio.h>

namespace paperos {

static const char* kDirs[] = {
    "/paperos", "/paperos/books", "/paperos/wallpapers",
    "/paperos/cache", "/paperos/logs", "/paperos/fonts"
};

bool Storage::ensureLayout() {
    if (!sd_.present()) return false;
    for (auto* d : kDirs) if (!SD.exists(d)) SD.mkdir(d);
    return true;
}

void Storage::appendLog(const char* line) {
    if (!sd_.present()) return;
    const char* path = "/paperos/logs/paperos.0.log";
    File f = SD.open(path, FILE_APPEND);
    if (!f) { LOG_WARN("storage", "open log failed"); return; }
    if (f.size() + strlen(line) + 1 > kLogRotateBytes) {
        f.close();
        // rotate: 1.log -> 2.log, 0.log -> 1.log, then truncate 0.log
        SD.remove("/paperos/logs/paperos.2.log");
        SD.rename("/paperos/logs/paperos.1.log", "/paperos/logs/paperos.2.log");
        SD.rename("/paperos/logs/paperos.0.log", "/paperos/logs/paperos.1.log");
        f = SD.open(path, FILE_WRITE);
        if (!f) { LOG_WARN("storage", "reopen log after rotate failed"); return; }
    }
    f.println(line);
    f.close();
}

bool Storage::hasBootstrapConfig() {
    return sd_.present() && SD.exists("/paperos/config.json");
}

} // namespace paperos
