#include "ConfigJson.h"
#include <ArduinoJson.h>
#include <cstring>

namespace paperos {

std::string serializeConfig(const Config& c) {
    JsonDocument doc;
    auto wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = c.wifiSsid; wifi["password"] = c.wifiPass;
    auto ha = doc["ha"].to<JsonObject>();
    ha["url"] = c.haUrl; ha["token"] = c.haToken;
    doc["timezone_offset_hours"] = c.tzOffsetHours;
    auto wx = doc["weather"].to<JsonObject>();
    wx["lat"] = c.weatherLat; wx["lon"] = c.weatherLon;
    wx["refresh_min"] = c.weatherRefreshMin; wx["indoor_offset"] = c.indoorTempOffset;
    auto rd = doc["reader"].to<JsonObject>();
    rd["font_size"] = c.fontSize; rd["margin_px"] = c.marginPx;
    auto ss = doc["screensaver"].to<JsonObject>();
    ss["idle_s"] = c.screensaverIdleS; ss["rotate_min"] = c.photoRotateMin;
    doc["language"] = (c.language == 1) ? "en" : "ru";

    std::string out;
    serializeJsonPretty(doc, out);
    return out;
}

bool parseConfigMerge(const std::string& json, Config& io) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    auto str = [&](const char* a, const char* b, std::string& dst) {
        const char* s = doc[a][b] | static_cast<const char*>(nullptr);
        if (s && *s) dst = s;
    };
    str("wifi", "ssid", io.wifiSsid);
    str("wifi", "password", io.wifiPass);
    str("ha", "url", io.haUrl);
    str("ha", "token", io.haToken);
    str("weather", "lat", io.weatherLat);
    str("weather", "lon", io.weatherLon);

    if (doc["timezone_offset_hours"].is<int>()) {
        int t = doc["timezone_offset_hours"].as<int>();
        if (t >= -11 && t <= 12) io.tzOffsetHours = static_cast<int8_t>(t);
    }
    if (doc["weather"]["refresh_min"].is<int>()) {
        int r = doc["weather"]["refresh_min"].as<int>();
        if (r >= 1 && r <= 60) io.weatherRefreshMin = static_cast<uint16_t>(r);
    }
    if (doc["weather"]["indoor_offset"].is<int>()) {
        int v = doc["weather"]["indoor_offset"].as<int>();
        if (v >= -20 && v <= 20) io.indoorTempOffset = static_cast<int8_t>(v);
    }
    if (doc["reader"]["font_size"].is<int>()) {
        int f = doc["reader"]["font_size"].as<int>();
        if (f >= 0 && f <= 2) io.fontSize = static_cast<uint8_t>(f);
    }
    if (doc["reader"]["margin_px"].is<int>()) {
        int m = doc["reader"]["margin_px"].as<int>();
        if (m >= 0 && m <= 100) io.marginPx = static_cast<uint8_t>(m);
    }
    if (doc["screensaver"]["idle_s"].is<int>()) {
        int i = doc["screensaver"]["idle_s"].as<int>();
        // upper bound = uint16_t max (the field type); otherwise the cast would silently truncate
        if (i >= 30 && i <= 65535) io.screensaverIdleS = static_cast<uint16_t>(i);
    }
    if (doc["screensaver"]["rotate_min"].is<int>()) {
        int p = doc["screensaver"]["rotate_min"].as<int>();
        if (p >= 1 && p <= 1440) io.photoRotateMin = static_cast<uint16_t>(p);
    }
    if (doc["language"].is<const char*>()) {
        const char* lng = doc["language"].as<const char*>();
        if (lng && !strcmp(lng, "en")) io.language = 1;
        else if (lng && !strcmp(lng, "ru")) io.language = 0;
    }
    return true;
}

} // namespace paperos
