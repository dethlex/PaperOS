#include "HaCalendarParse.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace paperos {

// Howard Hinnant's days-from-civil: дни от 1970-01-01 для GRIGORIAN даты.
static int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153u * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2u) / 5u
                   + static_cast<unsigned>(d) - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

uint32_t parseIsoDateToLocalEpoch(const char* s) {
    int y = 0, m = 0, d = 0;
    if (!s || std::sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3) return 0;
    if (m < 1 || m > 12 || d < 1 || d > 31) return 0;
    int64_t days = daysFromCivil(y, m, d);
    if (days < 0) return 0;
    return static_cast<uint32_t>(days * 86400);
}

uint32_t parseIso8601ToLocalEpoch(const char* s, int16_t tz_offset_min) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
    if (!s) return 0;
    int consumed = 0;
    if (std::sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &sec, &consumed) != 6)
        return 0;
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 60) return 0;

    // Суффикс: 'Z', либо ±HH:MM. Отсутствие суффикса трактуем как UTC (HA всегда
    // шлёт офсет, это только защита).
    long iso_off_sec = 0;
    const char* p = s + consumed;
    if (*p == '+' || *p == '-') {
        int oh = 0, om = 0;
        if (std::sscanf(p + 1, "%2d:%2d", &oh, &om) != 2) return 0;
        iso_off_sec = (oh * 3600L + om * 60L) * (*p == '-' ? -1 : 1);
    }
    int64_t days = daysFromCivil(y, mo, d);
    if (days < 0) return 0;
    int64_t utc = days * 86400 + h * 3600 + mi * 60 + sec - iso_off_sec;
    int64_t local = utc + static_cast<int64_t>(tz_offset_min) * 60;
    if (local < 0 || local > 0xFFFFFFFFLL) return 0;
    return static_cast<uint32_t>(local);
}

// Копирование UTF-8 строки в фикс. буфер без разрыва многобайтового символа.
static void copyUtf8Truncated(char* dst, size_t cap, const char* src) {
    size_t n = std::strlen(src);
    if (n < cap) { std::memcpy(dst, src, n + 1); return; }
    size_t cut = cap - 1;
    while (cut > 0 && (static_cast<uint8_t>(src[cut]) & 0xC0) == 0x80) cut--;
    std::memcpy(dst, src, cut);
    dst[cut] = 0;
}

bool parseHaCalendar(const std::string& json, uint32_t window_start_local,
                     int16_t tz_offset_min, CalendarData& out) {
    out = CalendarData{};
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    if (!doc.is<JsonArrayConst>()) return false;

    std::vector<CalendarEvent> evs;
    for (JsonObjectConst it : doc.as<JsonArrayConst>()) {
        CalendarEvent e;
        const char* sdt = it["start"]["dateTime"] | static_cast<const char*>(nullptr);
        const char* sd  = it["start"]["date"]     | static_cast<const char*>(nullptr);
        if (sdt) {
            e.all_day = false;
            e.start_local = parseIso8601ToLocalEpoch(sdt, tz_offset_min);
            const char* edt = it["end"]["dateTime"] | static_cast<const char*>(nullptr);
            e.end_local = edt ? parseIso8601ToLocalEpoch(edt, tz_offset_min) : e.start_local;
        } else if (sd) {
            e.all_day = true;
            e.start_local = parseIsoDateToLocalEpoch(sd);
            const char* ed = it["end"]["date"] | static_cast<const char*>(nullptr);
            e.end_local = ed ? parseIsoDateToLocalEpoch(ed) : e.start_local + 86400u;
        } else {
            continue;   // ни dateTime, ни date — мусор, пропускаем
        }
        if (e.start_local == 0) continue;                       // малформатное время
        if (e.end_local < e.start_local) e.end_local = e.start_local;
        if (e.end_local <= window_start_local) continue;        // закончилось до окна

        copyUtf8Truncated(e.title, sizeof(e.title), it["summary"] | "");
        copyUtf8Truncated(e.location, sizeof(e.location), it["location"] | "");
        evs.push_back(e);
    }

    std::sort(evs.begin(), evs.end(), [](const CalendarEvent& a, const CalendarEvent& b) {
        if (a.start_local != b.start_local) return a.start_local < b.start_local;
        return a.all_day && !b.all_day;   // all-day первыми при равном старте
    });

    size_t n = evs.size() > kMaxCalendarEvents ? kMaxCalendarEvents : evs.size();
    for (size_t i = 0; i < n; ++i) out.events[i] = evs[i];
    out.count = static_cast<uint8_t>(n);
    out.valid = true;
    return true;
}

} // namespace paperos
