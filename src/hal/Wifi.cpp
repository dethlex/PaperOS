#include "Wifi.h"
#include <WiFi.h>
#include "util/Logger.h"

namespace paperos {

WifiResult Wifi::connect(const std::string& ssid, const std::string& pass, uint32_t timeoutMs) {
    if (ssid.empty()) return WifiResult::NoSsid;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            LOG_INFO("wifi", "ok rssi=%d ip=%s", WiFi.RSSI(), WiFi.localIP().toString().c_str());
            return WifiResult::Ok;
        }
        if (st == WL_NO_SSID_AVAIL)    return WifiResult::NoSsid;
        if (st == WL_CONNECT_FAILED)   return WifiResult::AuthFail;
        delay(100);
    }
    WiFi.disconnect(true, true);
    return WifiResult::Timeout;
}

void Wifi::disconnect() { WiFi.disconnect(true, true); WiFi.mode(WIFI_OFF); }
bool Wifi::isConnected() const { return WiFi.status() == WL_CONNECTED; }
std::string Wifi::ipString() const { return WiFi.localIP().toString().c_str(); }
int Wifi::rssi() const { return WiFi.RSSI(); }

} // namespace paperos
