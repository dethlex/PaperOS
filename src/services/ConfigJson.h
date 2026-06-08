#pragma once
#include <string>
#include <cstdint>

namespace paperos {

// Пользовательский конфиг (без runtime-состояния). Дефолты = дефолты ConfigStore.
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

// Полный конфиг → JSON (вложенная схема).
std::string serializeConfig(const Config& c);

// Накладывает на `io` присутствующие НЕПУСТЫЕ строки и присутствующие числа
// (с валидацией диапазона). Пустые строки и отсутствующие ключи `io` не меняют.
// Возвращает false при ошибке разбора JSON (io не трогается).
bool parseConfigMerge(const std::string& json, Config& io);

} // namespace paperos
