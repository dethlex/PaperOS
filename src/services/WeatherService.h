#pragma once
#include "services/WeatherData.h"
#include "services/HttpClient.h"
#include <string>

namespace paperos {

class Wifi;
class ConfigStore;

// Networking facade, mirrors HAClient. Owns the HttpClient; constructs the
// concrete provider (Open-Meteo) per fetch(). Reads lat/lon from ConfigStore
// in begin(). Available both to the Weather app and to main.cpp's lock-screen
// refresh path (via AppContext). Callers must call begin() before fetch()
// (same lifecycle contract as HAClient); loadCache()/saveCache() need no WiFi.
class WeatherService {
public:
    WeatherService(Wifi& w, ConfigStore& c) : wifi_(w), cfg_(c) {}

    void begin();                         // read lat/lon, set HTTP timeout
    bool ensureWifi();                    // connect if not already (~8s once)
    void releaseWifi();
    bool fetch(WeatherData& out);         // network round-trip; requires begin() first
    bool loadCache(WeatherData& out);     // instant, from NVS, no network
    void saveCache(const WeatherData& d); // to NVS

private:
    Wifi& wifi_;
    ConfigStore& cfg_;
    HttpClient http_;
    double lat_ = 0, lon_ = 0;
};

} // namespace paperos
