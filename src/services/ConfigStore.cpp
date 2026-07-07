#include "ConfigStore.h"
#include "hal/Nvs.h"
#include "hal/Sd.h"
#include <vector>
#include <stdio.h>

namespace paperos {

static constexpr const char* kConfigPath = "/paperos/config.json";

// Config fields: after writing to NVS, mirror to the file (if enabled).
std::string ConfigStore::wifiSsid()      { return nvs_.getString("wifi_ssid"); }
void ConfigStore::setWifiSsid(const std::string& v) { nvs_.putString("wifi_ssid", v); if (persist_enabled_) persistToFile(); }
std::string ConfigStore::wifiPass()      { return nvs_.getString("wifi_pass"); }
void ConfigStore::setWifiPass(const std::string& v) { nvs_.putString("wifi_pass", v); if (persist_enabled_) persistToFile(); }

std::string ConfigStore::haUrl()         { return nvs_.getString("ha_url"); }
void ConfigStore::setHaUrl(const std::string& v) { nvs_.putString("ha_url", v); if (persist_enabled_) persistToFile(); }
std::string ConfigStore::haToken()       { return nvs_.getString("ha_token"); }
void ConfigStore::setHaToken(const std::string& v) { nvs_.putString("ha_token", v); if (persist_enabled_) persistToFile(); }

std::string ConfigStore::printerUrl()       { return nvs_.getString("pr_url"); }
void ConfigStore::setPrinterUrl(const std::string& v) { nvs_.putString("pr_url", v); if (persist_enabled_) persistToFile(); }
std::string ConfigStore::printerApiKey()    { return nvs_.getString("pr_key"); }
void ConfigStore::setPrinterApiKey(const std::string& v) { nvs_.putString("pr_key", v); if (persist_enabled_) persistToFile(); }
std::string ConfigStore::printerLightName() { return nvs_.getString("pr_led"); }
void ConfigStore::setPrinterLightName(const std::string& v) { nvs_.putString("pr_led", v); if (persist_enabled_) persistToFile(); }
std::string ConfigStore::printerLightOn()   { return nvs_.getString("pr_on"); }
void ConfigStore::setPrinterLightOn(const std::string& v) { nvs_.putString("pr_on", v); if (persist_enabled_) persistToFile(); }
std::string ConfigStore::printerLightOff()  { return nvs_.getString("pr_off"); }
void ConfigStore::setPrinterLightOff(const std::string& v) { nvs_.putString("pr_off", v); if (persist_enabled_) persistToFile(); }

std::string ConfigStore::printerPowerDevice() { return nvs_.getString("pr_pwr"); }
void ConfigStore::setPrinterPowerDevice(const std::string& v) { nvs_.putString("pr_pwr", v); if (persist_enabled_) persistToFile(); }
uint16_t ConfigStore::printerPreheatNozzle() { return nvs_.getU16("pr_pn", 200); }
void ConfigStore::setPrinterPreheatNozzle(uint16_t v) { nvs_.putU16("pr_pn", v); if (persist_enabled_) persistToFile(); }
uint16_t ConfigStore::printerPreheatBed() { return nvs_.getU16("pr_pb", 60); }
void ConfigStore::setPrinterPreheatBed(uint16_t v) { nvs_.putU16("pr_pb", v); if (persist_enabled_) persistToFile(); }

uint8_t ConfigStore::fontSize()          { return nvs_.getU8("font_size", kDefaultFontSize); }
void ConfigStore::setFontSize(uint8_t v) { nvs_.putU8("font_size", v); if (persist_enabled_) persistToFile(); }
uint8_t ConfigStore::marginPx()          { return nvs_.getU8("margin_px", kDefaultMarginPx); }
void ConfigStore::setMarginPx(uint8_t v) { nvs_.putU8("margin_px", v); if (persist_enabled_) persistToFile(); }

uint16_t ConfigStore::screensaverIdleS() { return nvs_.getU16("ss_idle_s", kDefaultIdleS); }
void ConfigStore::setScreensaverIdleS(uint16_t s) { nvs_.putU16("ss_idle_s", s); if (persist_enabled_) persistToFile(); }
uint16_t ConfigStore::photoRotateMin()   { return nvs_.getU16("photo_min", kDefaultRotateMin); }
void ConfigStore::setPhotoRotateMin(uint16_t m) { nvs_.putU16("photo_min", m); if (persist_enabled_) persistToFile(); }

int8_t ConfigStore::tzOffsetHours()      { return static_cast<int8_t>(nvs_.getU8("tz_off", static_cast<uint8_t>(kDefaultTzOffsetH))); }
void ConfigStore::setTzOffsetHours(int8_t h) { nvs_.putU8("tz_off", static_cast<uint8_t>(h)); if (persist_enabled_) persistToFile(); }

uint8_t ConfigStore::language()          { return nvs_.getU8("lang", 1); }  // default 1 = En
void ConfigStore::setLanguage(uint8_t v) { nvs_.putU8("lang", v); if (persist_enabled_) persistToFile(); }

std::string ConfigStore::weatherLat()      { return nvs_.getString("wx_lat"); }
void ConfigStore::setWeatherLat(const std::string& v) { nvs_.putString("wx_lat", v); if (persist_enabled_) persistToFile(); }
std::string ConfigStore::weatherLon()      { return nvs_.getString("wx_lon"); }
void ConfigStore::setWeatherLon(const std::string& v) { nvs_.putString("wx_lon", v); if (persist_enabled_) persistToFile(); }

uint16_t ConfigStore::weatherRefreshMin()  { return nvs_.getU16("wx_refresh", kDefaultWeatherRefreshMin); }
void ConfigStore::setWeatherRefreshMin(uint16_t m) { nvs_.putU16("wx_refresh", m); if (persist_enabled_) persistToFile(); }

int8_t ConfigStore::indoorTempOffset()  { return static_cast<int8_t>(nvs_.getU8("in_toff", static_cast<uint8_t>(kDefaultIndoorTempOffset))); }
void ConfigStore::setIndoorTempOffset(int8_t v) { nvs_.putU8("in_toff", static_cast<uint8_t>(v)); if (persist_enabled_) persistToFile(); }

std::string ConfigStore::calendarEntity()  { return nvs_.getString("cal_entity"); }
void ConfigStore::setCalendarEntity(const std::string& v) { nvs_.putString("cal_entity", v); if (persist_enabled_) persistToFile(); }

// Runtime fields: NOT mirrored (high-frequency / internal).
std::string ConfigStore::lastBookPath()  { return nvs_.getString("last_book"); }
void ConfigStore::setLastBookPath(const std::string& v) { nvs_.putString("last_book", v); }
uint32_t ConfigStore::lastBookOffset()   { return nvs_.getU32("last_off", 0); }
void ConfigStore::setLastBookOffset(uint32_t o) { nvs_.putU32("last_off", o); }

static void entityKey(const char* eid, char* out, size_t n) {
    snprintf(out, n, "es_%.12s", eid);
}
void ConfigStore::putEntityState(const char* eid, const void* data, size_t len) {
    char k[16]; entityKey(eid, k, sizeof(k)); nvs_.putBytes(k, data, len);
}
size_t ConfigStore::getEntityState(const char* eid, void* out, size_t maxLen) {
    char k[16]; entityKey(eid, k, sizeof(k)); return nvs_.getBytes(k, out, maxLen);
}

void   ConfigStore::putWeatherCache(const void* d, size_t n) { nvs_.putBytes("wx_cache", d, n); }
size_t ConfigStore::getWeatherCache(void* o, size_t n)       { return nvs_.getBytes("wx_cache", o, n); }

void   ConfigStore::putPrinterCache(const void* d, size_t n) { nvs_.putBytes("pr_cache", d, n); }
size_t ConfigStore::getPrinterCache(void* o, size_t n)       { return nvs_.getBytes("pr_cache", o, n); }

void   ConfigStore::putCalendarCache(const void* d, size_t n) { nvs_.putBytes("cal_cache", d, n); }
size_t ConfigStore::getCalendarCache(void* o, size_t n)       { return nvs_.getBytes("cal_cache", o, n); }

uint16_t ConfigStore::photoIndex()          { return nvs_.getU16("photo_idx", 0); }
void ConfigStore::setPhotoIndex(uint16_t v) { nvs_.putU16("photo_idx", v); }
uint8_t ConfigStore::lastAppIndex()         { return nvs_.getU8("last_app", 0); }
void ConfigStore::setLastAppIndex(uint8_t v){ nvs_.putU8("last_app", v); }

// --- config.json mirror ---

Config ConfigStore::snapshot() {
    Config c;
    c.wifiSsid = wifiSsid(); c.wifiPass = wifiPass();
    c.haUrl = haUrl(); c.haToken = haToken();
    c.tzOffsetHours = tzOffsetHours();
    c.weatherLat = weatherLat(); c.weatherLon = weatherLon();
    c.weatherRefreshMin = weatherRefreshMin();
    c.indoorTempOffset = indoorTempOffset();
    c.fontSize = fontSize(); c.marginPx = marginPx();
    c.screensaverIdleS = screensaverIdleS(); c.photoRotateMin = photoRotateMin();
    c.language = language();
    c.printerUrl = printerUrl(); c.printerApiKey = printerApiKey();
    c.printerLightName = printerLightName();
    c.printerLightOn = printerLightOn(); c.printerLightOff = printerLightOff();
    c.printerPowerDevice = printerPowerDevice();
    c.printerPreheatNozzle = printerPreheatNozzle();
    c.printerPreheatBed = printerPreheatBed();
    c.calendarEntity = calendarEntity();
    return c;
}

void ConfigStore::applyConfig(const Config& c) {
    persist_enabled_ = false;        // single file write done externally, not per-setter
    setWifiSsid(c.wifiSsid); setWifiPass(c.wifiPass);
    setHaUrl(c.haUrl); setHaToken(c.haToken);
    setPrinterUrl(c.printerUrl); setPrinterApiKey(c.printerApiKey);
    setPrinterLightName(c.printerLightName);
    setPrinterLightOn(c.printerLightOn); setPrinterLightOff(c.printerLightOff);
    setPrinterPowerDevice(c.printerPowerDevice);
    setPrinterPreheatNozzle(static_cast<uint16_t>(c.printerPreheatNozzle));
    setPrinterPreheatBed(static_cast<uint16_t>(c.printerPreheatBed));
    setTzOffsetHours(c.tzOffsetHours);
    setWeatherLat(c.weatherLat); setWeatherLon(c.weatherLon);
    setWeatherRefreshMin(c.weatherRefreshMin);
    setIndoorTempOffset(c.indoorTempOffset);
    setFontSize(c.fontSize); setMarginPx(c.marginPx);
    setScreensaverIdleS(c.screensaverIdleS); setPhotoRotateMin(c.photoRotateMin);
    setLanguage(c.language);
    setCalendarEntity(c.calendarEntity);
    persist_enabled_ = true;
}

void ConfigStore::persistToFile() {
    if (!sd_.present()) return;
    std::string j = serializeConfig(snapshot());
    sd_.writeAtomic(kConfigPath, reinterpret_cast<const uint8_t*>(j.data()), j.size());
}

bool ConfigStore::syncWithFile() {
    if (!sd_.present()) return false;
    Config cfg = snapshot();
    std::vector<uint8_t> buf;
    size_t n = sd_.readAll(kConfigPath, buf);     // 0 if file is missing/empty/error
    if (n > 0) {
        std::string json(reinterpret_cast<const char*>(buf.data()), n);
        if (!parseConfigMerge(json, cfg)) return false;   // malformed JSON: leave NVS and file untouched
        applyConfig(cfg);                                  // non-empty file fields -> NVS
    }
    std::string out = serializeConfig(cfg);                // write full config back (creates the file if absent)
    sd_.writeAtomic(kConfigPath, reinterpret_cast<const uint8_t*>(out.data()), out.size());
    return true;
}

} // namespace paperos
