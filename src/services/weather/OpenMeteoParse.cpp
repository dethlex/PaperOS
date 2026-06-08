#include "OpenMeteoParse.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

namespace paperos {

// Sakamoto's algorithm. Returns 0=Mon … 6=Sun for a Gregorian date.
static uint8_t weekdayMon0(int y, int m, int d) {
    static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 3) y -= 1;
    int sun0 = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;  // 0=Sun … 6=Sat
    return static_cast<uint8_t>((sun0 + 6) % 7);            // shift to 0=Mon
}

static int16_t roundC(double v) {
    double r = v >= 0 ? v + 0.5 : v - 0.5;
    if (r >  32767.0) return  32767;
    if (r < -32768.0) return -32768;
    return static_cast<int16_t>(r);
}

bool parseOpenMeteo(const std::string& json, WeatherData& out) {
    out = WeatherData{};
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    if (!doc["current"].is<JsonObjectConst>()) return false;

    out.current_c   = roundC(doc["current"]["temperature_2m"] | 0.0);
    out.current_wmo = static_cast<uint8_t>(doc["current"]["weather_code"] | 0);
    out.current_humidity = static_cast<uint8_t>(doc["current"]["relative_humidity_2m"] | 0);

    JsonArrayConst times = doc["daily"]["time"].as<JsonArrayConst>();
    JsonArrayConst codes = doc["daily"]["weather_code"].as<JsonArrayConst>();
    JsonArrayConst tmax  = doc["daily"]["temperature_2m_max"].as<JsonArrayConst>();
    JsonArrayConst tmin  = doc["daily"]["temperature_2m_min"].as<JsonArrayConst>();

    size_t n = times.size();
    if (n > WeatherData::kMaxDays) n = WeatherData::kMaxDays;
    for (size_t i = 0; i < n; ++i) {
        DayForecast& d = out.days[i];
        d.wmo_code = static_cast<uint8_t>(codes[i] | 0);
        d.tmax_c   = roundC(tmax[i] | 0.0);
        d.tmin_c   = roundC(tmin[i] | 0.0);
        const char* date = times[i] | "";
        int Y = 0, M = 0, D = 0;
        if (sscanf(date, "%d-%d-%d", &Y, &M, &D) == 3 && M >= 1 && M <= 12)
            d.weekday = weekdayMon0(Y, M, D);
    }
    out.day_count = static_cast<uint8_t>(n);

    JsonArrayConst htimes = doc["hourly"]["time"].as<JsonArrayConst>();
    JsonArrayConst htemp  = doc["hourly"]["temperature_2m"].as<JsonArrayConst>();
    JsonArrayConst hcodes = doc["hourly"]["weather_code"].as<JsonArrayConst>();
    size_t hn = htimes.size();
    if (hn > WeatherData::kMaxHours) hn = WeatherData::kMaxHours;
    for (size_t i = 0; i < hn; ++i) {
        HourForecast& h = out.hours[i];
        const char* ts = htimes[i] | "";              // "2026-06-01T14:00"
        const char* tp = strchr(ts, 'T');
        if (tp && tp[1] >= '0' && tp[1] <= '9' && tp[2] >= '0' && tp[2] <= '9')
            h.hour = static_cast<uint8_t>((tp[1] - '0') * 10 + (tp[2] - '0'));
        h.temp_c   = roundC(htemp[i] | 0.0);
        h.wmo_code = static_cast<uint8_t>(hcodes[i] | 0);
    }
    out.hour_count = static_cast<uint8_t>(hn);

    out.valid = true;
    return true;
}

} // namespace paperos
