#include "WeatherCache.h"
#include <cstring>

namespace paperos {

// Layout v2:
//   [0] version=2  [1] day_count  [2..3] current_c  [4] current_wmo
//   [5] current_humidity  [6..9] fetched_unix  [10] hour_count
//   then day_count × DayForecast (6 B): tmax(2) tmin(2) wmo weekday
//   then hour_count × HourForecast (4 B): hour temp(2) wmo
static constexpr size_t kHeader  = 11;
static constexpr size_t kPerDay  = 6;
static constexpr size_t kPerHour = 4;

static int16_t readI16LE(const uint8_t* p) {
    uint16_t raw = static_cast<uint16_t>(p[0]) |
                   (static_cast<uint16_t>(p[1]) << 8);
    int16_t v;
    std::memcpy(&v, &raw, sizeof(v));
    return v;
}

size_t serializeWeather(const WeatherData& d, uint8_t* buf, size_t cap) {
    size_t need = kHeader + static_cast<size_t>(d.day_count) * kPerDay
                          + static_cast<size_t>(d.hour_count) * kPerHour;
    if (cap < need) return 0;
    size_t p = 0;
    buf[p++] = 2;
    buf[p++] = d.day_count;
    buf[p++] = static_cast<uint8_t>(static_cast<uint16_t>(d.current_c) & 0xFF);
    buf[p++] = static_cast<uint8_t>((static_cast<uint16_t>(d.current_c) >> 8) & 0xFF);
    buf[p++] = d.current_wmo;
    buf[p++] = d.current_humidity;
    buf[p++] = static_cast<uint8_t>(d.fetched_unix & 0xFF);
    buf[p++] = static_cast<uint8_t>((d.fetched_unix >> 8) & 0xFF);
    buf[p++] = static_cast<uint8_t>((d.fetched_unix >> 16) & 0xFF);
    buf[p++] = static_cast<uint8_t>((d.fetched_unix >> 24) & 0xFF);
    buf[p++] = d.hour_count;
    for (uint8_t i = 0; i < d.day_count; ++i) {
        const DayForecast& f = d.days[i];
        buf[p++] = static_cast<uint8_t>(static_cast<uint16_t>(f.tmax_c) & 0xFF);
        buf[p++] = static_cast<uint8_t>((static_cast<uint16_t>(f.tmax_c) >> 8) & 0xFF);
        buf[p++] = static_cast<uint8_t>(static_cast<uint16_t>(f.tmin_c) & 0xFF);
        buf[p++] = static_cast<uint8_t>((static_cast<uint16_t>(f.tmin_c) >> 8) & 0xFF);
        buf[p++] = f.wmo_code;
        buf[p++] = f.weekday;
    }
    for (uint8_t i = 0; i < d.hour_count; ++i) {
        const HourForecast& h = d.hours[i];
        buf[p++] = h.hour;
        buf[p++] = static_cast<uint8_t>(static_cast<uint16_t>(h.temp_c) & 0xFF);
        buf[p++] = static_cast<uint8_t>((static_cast<uint16_t>(h.temp_c) >> 8) & 0xFF);
        buf[p++] = h.wmo_code;
    }
    return p;
}

bool deserializeWeather(const uint8_t* buf, size_t len, WeatherData& out) {
    out = WeatherData{};
    if (!buf || len < kHeader || buf[0] != 2) return false;
    uint8_t dc = buf[1];
    if (dc > WeatherData::kMaxDays) return false;
    uint8_t hc = buf[10];
    if (hc > WeatherData::kMaxHours) return false;
    if (len < kHeader + static_cast<size_t>(dc) * kPerDay
                      + static_cast<size_t>(hc) * kPerHour) return false;

    out.day_count = dc;
    out.hour_count = hc;
    out.current_c        = readI16LE(buf + 2);
    out.current_wmo      = buf[4];
    out.current_humidity = buf[5];
    out.fetched_unix = static_cast<uint32_t>(buf[6]) |
                       (static_cast<uint32_t>(buf[7]) << 8) |
                       (static_cast<uint32_t>(buf[8]) << 16) |
                       (static_cast<uint32_t>(buf[9]) << 24);
    size_t p = kHeader;
    for (uint8_t i = 0; i < dc; ++i) {
        DayForecast& f = out.days[i];
        f.tmax_c = readI16LE(buf + p); p += 2;
        f.tmin_c = readI16LE(buf + p); p += 2;
        f.wmo_code = buf[p++];
        f.weekday  = buf[p++];
    }
    for (uint8_t i = 0; i < hc; ++i) {
        HourForecast& h = out.hours[i];
        h.hour = buf[p++];
        h.temp_c = readI16LE(buf + p); p += 2;
        h.wmo_code = buf[p++];
    }
    out.valid = true;
    return true;
}

} // namespace paperos
