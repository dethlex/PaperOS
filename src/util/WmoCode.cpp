#include "WmoCode.h"

namespace paperos {

WeatherCat catFromWmo(uint8_t c) {
    switch (c) {
        case 0:  return WeatherCat::Clear;
        case 1:
        case 2:  return WeatherCat::PartlyCloudy;
        case 3:  return WeatherCat::Cloudy;
        case 45:
        case 48: return WeatherCat::Fog;
        case 51: case 53: case 55: case 56: case 57:
        case 61: case 63: case 65: case 66: case 67:
        case 80: case 81: case 82: return WeatherCat::Rain;
        case 71: case 73: case 75: case 77:
        case 85: case 86: return WeatherCat::Snow;
        case 95: case 96: case 99: return WeatherCat::Thunder;
        default: return WeatherCat::Unknown;
    }
}

Str wmoLabelId(WeatherCat cat) {
    switch (cat) {
        case WeatherCat::Clear:        return Str::wmo_clear;
        case WeatherCat::PartlyCloudy: return Str::wmo_partly;
        case WeatherCat::Cloudy:       return Str::wmo_cloudy;
        case WeatherCat::Fog:          return Str::wmo_fog;
        case WeatherCat::Rain:         return Str::wmo_rain;
        case WeatherCat::Snow:         return Str::wmo_snow;
        case WeatherCat::Thunder:      return Str::wmo_thunder;
        default:                       return Str::wmo_unknown;
    }
}

} // namespace paperos
