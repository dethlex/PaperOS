#pragma once
#include "services/WeatherData.h"

namespace paperos {

struct WeatherQuery { double lat = 0; double lon = 0; };

// Strategy interface. Only OpenMeteoProvider exists today; a HaWeatherProvider
// is a documented TODO and will implement this same interface.
class WeatherProvider {
public:
    virtual ~WeatherProvider() = default;
    virtual bool fetch(const WeatherQuery& q, WeatherData& out) = 0;
};

} // namespace paperos
