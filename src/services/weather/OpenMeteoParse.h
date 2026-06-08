#pragma once
#include "services/WeatherData.h"
#include <string>

namespace paperos {

// Parse an Open-Meteo /v1/forecast response body into WeatherData.
// Returns false on malformed JSON or a missing "current" object.
bool parseOpenMeteo(const std::string& json, WeatherData& out);

} // namespace paperos
