#pragma once
#include <string>
#include <stdint.h>
#include "services/ConfigJson.h"

namespace paperos {

class Nvs;
class Sd;

// Typed facade over Nvs. Config fields are mirrored to /paperos/config.json
// (auto-save on change + bidirectional syncWithFile). Runtime fields live in NVS only.
class ConfigStore {
public:
    ConfigStore(Nvs& nvs, Sd& sd) : nvs_(nvs), sd_(sd) {}

    // Wifi
    std::string wifiSsid();   void setWifiSsid(const std::string&);
    std::string wifiPass();   void setWifiPass(const std::string&);

    // Home Assistant
    std::string haUrl();      void setHaUrl(const std::string&);
    std::string haToken();    void setHaToken(const std::string&);

    // 3D printer (Moonraker)
    std::string printerUrl();       void setPrinterUrl(const std::string&);
    std::string printerApiKey();    void setPrinterApiKey(const std::string&);
    std::string printerLightName(); void setPrinterLightName(const std::string&);
    std::string printerLightOn();   void setPrinterLightOn(const std::string&);
    std::string printerLightOff();  void setPrinterLightOff(const std::string&);
    std::string printerPowerDevice();  void setPrinterPowerDevice(const std::string&);
    uint16_t printerPreheatNozzle();   void setPrinterPreheatNozzle(uint16_t);
    uint16_t printerPreheatBed();      void setPrinterPreheatBed(uint16_t);

    // Reader
    uint8_t fontSize();       void setFontSize(uint8_t v);     // 0=S,1=M,2=L
    uint8_t marginPx();       void setMarginPx(uint8_t v);

    // Screensaver
    uint16_t screensaverIdleS();  void setScreensaverIdleS(uint16_t s);
    uint16_t photoRotateMin();    void setPhotoRotateMin(uint16_t m);

    // Time
    int8_t tzOffsetHours();   void setTzOffsetHours(int8_t h);

    // Language (0 = Ru, 1 = En)
    uint8_t language();       void setLanguage(uint8_t v);

    // Weather
    std::string weatherLat();        void setWeatherLat(const std::string&);
    std::string weatherLon();        void setWeatherLon(const std::string&);
    uint16_t    weatherRefreshMin(); void setWeatherRefreshMin(uint16_t m);
    int8_t      indoorTempOffset();  void setIndoorTempOffset(int8_t v);
    void   putWeatherCache(const void* data, size_t len);
    size_t getWeatherCache(void* out, size_t maxLen);

    // Calendar (HA calendar entity)
    std::string calendarEntity();    void setCalendarEntity(const std::string&);
    void   putCalendarCache(const void* data, size_t len);   // NOT mirrored
    size_t getCalendarCache(void* out, size_t maxLen);

    // Boot/runtime state — NOT mirrored to config.json.
    uint16_t photoIndex();    void setPhotoIndex(uint16_t v);
    uint8_t  lastAppIndex();  void setLastAppIndex(uint8_t v);

    // Reader bookmark — NOT mirrored.
    std::string lastBookPath();   void setLastBookPath(const std::string&);
    uint32_t    lastBookOffset(); void setLastBookOffset(uint32_t off);

    // HA per-entity state cache — NOT mirrored.
    void putEntityState(const char* entityId, const void* data, size_t len);
    size_t getEntityState(const char* entityId, void* out, size_t maxLen);

    // Printer status cache — NOT mirrored.
    void   putPrinterCache(const void* data, size_t len);
    size_t getPrinterCache(void* out, size_t maxLen);

    // --- config.json mirror ---
    Config snapshot();                 // read all config fields from NVS
    void   applyConfig(const Config&); // write all config fields to NVS (no per-setter save)
    bool   syncWithFile();             // file<->NVS (boot + button). false: no SD or malformed JSON.

    // Defaults
    static constexpr uint8_t  kDefaultFontSize   = 1;
    static constexpr uint8_t  kDefaultMarginPx   = 24;
    static constexpr uint16_t kDefaultIdleS      = 300;
    static constexpr uint16_t kDefaultRotateMin  = 30;
    static constexpr int8_t   kDefaultTzOffsetH  = 3;
    static constexpr uint16_t kDefaultWeatherRefreshMin = 30;
    static constexpr int8_t   kDefaultIndoorTempOffset = -2;

private:
    void persistToFile();              // serializeConfig(snapshot()) -> writeAtomic (no-op without SD)
    Nvs& nvs_;
    Sd&  sd_;
    bool persist_enabled_ = true;      // suppresses auto-save during applyConfig
};

} // namespace paperos
