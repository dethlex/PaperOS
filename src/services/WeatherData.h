#pragma once
#include <stdint.h>

namespace paperos {

struct DayForecast {
    int16_t tmax_c   = 0;
    int16_t tmin_c   = 0;
    uint8_t wmo_code = 0;
    uint8_t weekday  = 0;   // 0=Пн … 6=Вс
};

struct HourForecast {
    uint8_t hour     = 0;   // 0..23 (local, from hourly.time)
    int16_t temp_c   = 0;
    uint8_t wmo_code = 0;
};

struct WeatherData {
    bool        valid            = false;
    int16_t     current_c        = 0;
    uint8_t     current_wmo      = 0;
    uint8_t     current_humidity = 0;   // % (relative_humidity_2m)
    uint8_t     day_count        = 0;   // ≤ kMaxDays
    DayForecast days[5];
    uint8_t     hour_count       = 0;   // ≤ kMaxHours
    HourForecast hours[12];
    uint32_t    fetched_unix     = 0;   // set by caller (has RTC); 0 if unknown
    static constexpr uint8_t kMaxDays  = 5;
    static constexpr uint8_t kMaxHours = 12;
};

} // namespace paperos
