#pragma once
#include <string>

namespace paperos {

enum class WifiResult { Ok, Timeout, NoSsid, AuthFail };

class Wifi {
public:
    // Blocking connect with timeout. Returns Ok on success.
    WifiResult connect(const std::string& ssid, const std::string& pass, uint32_t timeoutMs = 8000);
    void disconnect();
    bool isConnected() const;
    std::string ipString() const;
    int rssi() const;
};

} // namespace paperos
