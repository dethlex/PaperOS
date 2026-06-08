#pragma once
#include "services/weather/WeatherProvider.h"

namespace paperos {

class HttpClient;

class OpenMeteoProvider : public WeatherProvider {
public:
    explicit OpenMeteoProvider(HttpClient& http) : http_(http) {}
    bool fetch(const WeatherQuery& q, WeatherData& out) override;
private:
    HttpClient& http_;
};

} // namespace paperos
