#pragma once
#include <stdint.h>
#include "i18n/Strings.h"

namespace paperos {

// Normalized weather category used to pick an icon + a label.
enum class WeatherCat : uint8_t {
    Clear, PartlyCloudy, Cloudy, Fog, Rain, Snow, Thunder, Unknown
};

WeatherCat catFromWmo(uint8_t code);   // WMO weather_code -> category
Str        wmoLabelId(WeatherCat cat); // category -> Str id (caller does tr())

} // namespace paperos
