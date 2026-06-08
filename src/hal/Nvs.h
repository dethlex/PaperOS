#pragma once
#include <Preferences.h>
#include <string>

namespace paperos {

class Nvs {
public:
    bool begin(const char* ns = "paperos");

    bool     putString(const char* key, const std::string& v);
    std::string getString(const char* key, const std::string& def = {});

    bool     putU8 (const char* key, uint8_t v);
    uint8_t  getU8 (const char* key, uint8_t def = 0);
    bool     putU16(const char* key, uint16_t v);
    uint16_t getU16(const char* key, uint16_t def = 0);
    bool     putU32(const char* key, uint32_t v);
    uint32_t getU32(const char* key, uint32_t def = 0);
    bool     putU64(const char* key, uint64_t v);
    uint64_t getU64(const char* key, uint64_t def = 0);

    bool     putBytes(const char* key, const void* data, size_t len);
    size_t   getBytes(const char* key, void* out, size_t maxLen);

    bool     remove(const char* key);
    bool     clear();
private:
    Preferences prefs_;
};

} // namespace paperos
