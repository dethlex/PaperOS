#include "CalendarService.h"
#include "hal/Wifi.h"
#include "hal/Rtc.h"
#include "services/ConfigStore.h"
#include "services/CalendarCache.h"
#include "services/calendar/HaCalendarParse.h"
#include "util/Logger.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <ctime>

namespace paperos {

void CalendarService::begin() {
    url_    = cfg_.haUrl();
    token_  = cfg_.haToken();
    entity_ = cfg_.calendarEntity();
    http_.setBearerToken(token_);
    http_.setTimeoutMs(6000);
}

bool CalendarService::configured() const {
    return !url_.empty() && !token_.empty() && !entity_.empty();
}

bool CalendarService::ensureWifi() {
    if (wifi_.isConnected()) return true;
    return wifi_.connect(cfg_.wifiSsid(), cfg_.wifiPass(), 8000) == WifiResult::Ok;
}

void CalendarService::releaseWifi() { wifi_.disconnect(); }

std::string CalendarService::isoUtcFromLocal(uint32_t local_epoch) const {
    int64_t utc = static_cast<int64_t>(local_epoch)
                - static_cast<int64_t>(cfg_.tzOffsetHours()) * 3600;
    if (utc < 0) utc = 0;
    time_t t = static_cast<time_t>(utc);
    struct tm g;
    gmtime_r(&t, &g);
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
             g.tm_hour, g.tm_min, g.tm_sec);
    return buf;
}

bool CalendarService::fetch(CalendarData& out) {
    if (!configured()) return false;
    uint32_t now = static_cast<uint32_t>(rtc_.nowUnix());
    uint32_t today0 = now - now % 86400u;
    uint32_t end    = today0 + static_cast<uint32_t>(kWindowDays) * 86400u;

    std::string u = url_ + "/api/calendars/" + entity_ +
                    "?start=" + isoUtcFromLocal(today0) +
                    "&end="   + isoUtcFromLocal(end);
    auto resp = http_.get(u);
    if (!resp.ok()) {
        LOG_WARN("cal", "GET events -> %d %s", resp.status_code, resp.error.c_str());
        return false;
    }
    int16_t tz_min = static_cast<int16_t>(cfg_.tzOffsetHours()) * 60;
    if (!parseHaCalendar(resp.body, today0, tz_min, out)) {
        LOG_WARN("cal", "parse failed (%u bytes)", static_cast<unsigned>(resp.body.size()));
        return false;
    }
    out.fetched_unix = now;
    return true;
}

bool CalendarService::fetchCalendarList(std::vector<CalendarInfo>& out) {
    out.clear();
    if (url_.empty() || token_.empty()) return false;
    auto resp = http_.get(url_ + "/api/calendars");
    if (!resp.ok()) {
        LOG_WARN("cal", "GET /api/calendars -> %d", resp.status_code);
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body)) return false;
    if (!doc.is<JsonArrayConst>()) return false;
    for (JsonObjectConst it : doc.as<JsonArrayConst>()) {
        CalendarInfo ci;
        ci.entity_id = it["entity_id"] | "";
        ci.name      = it["name"] | "";
        if (!ci.entity_id.empty()) out.push_back(ci);
    }
    return true;
}

bool CalendarService::loadCache(CalendarData& out) {
    uint8_t buf[2900];
    size_t n = cfg_.getCalendarCache(buf, sizeof(buf));
    return deserializeCalendar(buf, n, out);
}

void CalendarService::saveCache(const CalendarData& d) {
    uint8_t buf[2900];
    size_t n = serializeCalendar(d, buf, sizeof(buf));
    if (n) cfg_.putCalendarCache(buf, n);
}

} // namespace paperos
