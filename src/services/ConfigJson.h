#pragma once
#include <string>
#include <cstdint>

namespace paperos {

// User config (no runtime state). Defaults match ConfigStore's defaults.
struct Config {
    std::string wifiSsid, wifiPass;
    std::string haUrl, haToken;
    int8_t      tzOffsetHours     = 3;
    std::string weatherLat, weatherLon;
    uint16_t    weatherRefreshMin = 30;
    int8_t      indoorTempOffset  = -2;
    uint8_t     fontSize          = 1;
    uint8_t     marginPx          = 24;
    uint16_t    screensaverIdleS  = 300;
    uint16_t    photoRotateMin    = 30;
    uint8_t     language          = 1;   // 0 = Ru, 1 = En (default En)
};

// Full config -> JSON (nested schema).
std::string serializeConfig(const Config& c);

// Merges into `io` the present NON-EMPTY strings and present numbers
// (range-validated). Empty strings and absent keys leave `io` unchanged.
// Returns false on JSON parse error (io is left untouched).
bool parseConfigMerge(const std::string& json, Config& io);

} // namespace paperos
