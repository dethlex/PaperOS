#include "OpenMeteoProvider.h"
#include "services/HttpClient.h"
#include "OpenMeteoParse.h"
#include "util/Logger.h"
#include <cstdio>

namespace paperos {

bool OpenMeteoProvider::fetch(const WeatherQuery& q, WeatherData& out) {
    char url[320];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code"
        "&hourly=temperature_2m,weather_code&forecast_hours=12"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min"
        "&forecast_days=5&timezone=auto",
        q.lat, q.lon);
    auto resp = http_.get(url);
    if (!resp.ok()) {
        LOG_WARN("wx", "GET open-meteo -> %d", resp.status_code);
        return false;
    }
    return parseOpenMeteo(resp.body, out);
}

} // namespace paperos
