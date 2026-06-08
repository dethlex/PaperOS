#include "Nvs.h"

namespace paperos {

bool Nvs::begin(const char* ns) { return prefs_.begin(ns, false); }

bool Nvs::putString(const char* k, const std::string& v) { return prefs_.putString(k, v.c_str()) > 0; }
std::string Nvs::getString(const char* k, const std::string& d) {
    return prefs_.isKey(k) ? prefs_.getString(k).c_str() : d;
}

bool Nvs::putU8 (const char* k, uint8_t v)  { return prefs_.putUChar(k, v) > 0; }
uint8_t Nvs::getU8 (const char* k, uint8_t d) { return prefs_.isKey(k) ? prefs_.getUChar(k) : d; }
bool Nvs::putU16(const char* k, uint16_t v) { return prefs_.putUShort(k, v) > 0; }
uint16_t Nvs::getU16(const char* k, uint16_t d){ return prefs_.isKey(k) ? prefs_.getUShort(k) : d; }
bool Nvs::putU32(const char* k, uint32_t v) { return prefs_.putUInt(k, v) > 0; }
uint32_t Nvs::getU32(const char* k, uint32_t d){ return prefs_.isKey(k) ? prefs_.getUInt(k) : d; }
bool Nvs::putU64(const char* k, uint64_t v) { return prefs_.putULong64(k, v) > 0; }
uint64_t Nvs::getU64(const char* k, uint64_t d){ return prefs_.isKey(k) ? prefs_.getULong64(k) : d; }

bool Nvs::putBytes(const char* k, const void* data, size_t len) { return prefs_.putBytes(k, data, len) == len; }
size_t Nvs::getBytes(const char* k, void* out, size_t maxLen)   { return prefs_.getBytes(k, out, maxLen); }

bool Nvs::remove(const char* k) { return prefs_.remove(k); }
bool Nvs::clear() { return prefs_.clear(); }

} // namespace paperos
