#include "WeatherService.h"
#include "hal/Wifi.h"
#include "services/ConfigStore.h"
#include "services/WeatherCache.h"
#include "services/weather/OpenMeteoProvider.h"
#include <cstdlib>

namespace paperos {

void WeatherService::begin() {
    lat_ = strtod(cfg_.weatherLat().c_str(), nullptr);
    lon_ = strtod(cfg_.weatherLon().c_str(), nullptr);
    http_.setTimeoutMs(6000);
}

bool WeatherService::ensureWifi() {
    if (wifi_.isConnected()) return true;
    return wifi_.connect(cfg_.wifiSsid(), cfg_.wifiPass(), 8000) == WifiResult::Ok;
}

void WeatherService::releaseWifi() { wifi_.disconnect(); }

bool WeatherService::fetch(WeatherData& out) {
    OpenMeteoProvider provider(http_);
    return provider.fetch(WeatherQuery{lat_, lon_}, out);
}

bool WeatherService::loadCache(WeatherData& out) {
    uint8_t buf[160];
    size_t n = cfg_.getWeatherCache(buf, sizeof(buf));
    return deserializeWeather(buf, n, out);
}

void WeatherService::saveCache(const WeatherData& d) {
    uint8_t buf[160];
    size_t n = serializeWeather(d, buf, sizeof(buf));
    if (n) cfg_.putWeatherCache(buf, n);
}

} // namespace paperos
