#include "Dashboard.h"
#include <ArduinoJson.h>
#include <string.h>

#ifndef PAPEROS_NATIVE
#include <SD.h>
#endif

namespace paperos {

static WidgetKind kindFor(const char* s) {
    if (strcmp(s, "toggle") == 0) return WidgetKind::Toggle;
    if (strcmp(s, "sensor") == 0) return WidgetKind::Sensor;
    if (strcmp(s, "action") == 0) return WidgetKind::Action;
    if (strcmp(s, "lock")   == 0) return WidgetKind::Lock;
    return WidgetKind::Sensor;   // unused sentinel; caller filters via isKnownKind
}
static bool isKnownKind(const char* s) {
    return !strcmp(s,"toggle") || !strcmp(s,"sensor") ||
           !strcmp(s,"action") || !strcmp(s,"lock");
}

bool Dashboard::parse(const char* json) {
    groups_.clear();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;
    if (!doc["groups"].is<JsonArray>()) return false;
    for (JsonObject g : doc["groups"].as<JsonArray>()) {
        DashboardGroup dg;
        dg.title = g["title"] | "";
        for (JsonObject it : g["items"].as<JsonArray>()) {
            const char* k = it["kind"] | "";
            if (!isKnownKind(k)) continue;
            DashboardItem di;
            di.entity_id = it["entity_id"] | "";
            di.label     = it["label"] | "";
            di.unit      = it["unit"] | "";
            di.kind      = kindFor(k);
            if (!di.entity_id.empty()) dg.items.push_back(di);
        }
        groups_.push_back(dg);
    }
    return true;
}

bool Dashboard::loadFromSd(const char* path) {
#ifdef PAPEROS_NATIVE
    (void)path; return false;
#else
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    std::string s; s.reserve(f.size());
    while (f.available()) s += static_cast<char>(f.read());
    f.close();
    return parse(s.c_str());
#endif
}

} // namespace paperos
