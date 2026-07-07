#include "CalendarCache.h"
#include <cstring>

namespace paperos {

// Layout v1:
//   [0] version=1  [1] count  [2..5] fetched_unix
//   then count × record (89 B): start(4) end(4) flags(1) title[48] location[32]
static constexpr size_t kHeader   = 6;
static constexpr size_t kPerEvent = 4 + 4 + 1 + 48 + 32;   // 89

static void writeU32LE(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}
static uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

size_t serializeCalendar(const CalendarData& d, uint8_t* buf, size_t cap) {
    uint8_t count = d.count > kMaxCalendarEvents ? kMaxCalendarEvents : d.count;
    size_t need = kHeader + static_cast<size_t>(count) * kPerEvent;
    if (cap < need) return 0;
    size_t p = 0;
    buf[p++] = 1;
    buf[p++] = count;
    writeU32LE(buf + p, d.fetched_unix); p += 4;
    for (uint8_t i = 0; i < count; ++i) {
        const CalendarEvent& e = d.events[i];
        writeU32LE(buf + p, e.start_local); p += 4;
        writeU32LE(buf + p, e.end_local);   p += 4;
        buf[p++] = e.all_day ? 1 : 0;
        std::memcpy(buf + p, e.title, sizeof(e.title));       p += sizeof(e.title);
        std::memcpy(buf + p, e.location, sizeof(e.location)); p += sizeof(e.location);
    }
    return p;
}

bool deserializeCalendar(const uint8_t* buf, size_t len, CalendarData& out) {
    out = CalendarData{};
    if (!buf || len < kHeader || buf[0] != 1) return false;
    uint8_t count = buf[1];
    if (count > kMaxCalendarEvents) return false;
    if (len < kHeader + static_cast<size_t>(count) * kPerEvent) return false;
    out.fetched_unix = readU32LE(buf + 2);
    size_t p = kHeader;
    for (uint8_t i = 0; i < count; ++i) {
        CalendarEvent& e = out.events[i];
        e.start_local = readU32LE(buf + p); p += 4;
        e.end_local   = readU32LE(buf + p); p += 4;
        e.all_day = buf[p++] != 0;
        std::memcpy(e.title, buf + p, sizeof(e.title));       p += sizeof(e.title);
        e.title[sizeof(e.title) - 1] = 0;                     // защита от незавершённой строки
        std::memcpy(e.location, buf + p, sizeof(e.location)); p += sizeof(e.location);
        e.location[sizeof(e.location) - 1] = 0;
    }
    out.count = count;
    out.valid = true;
    return true;
}

} // namespace paperos
