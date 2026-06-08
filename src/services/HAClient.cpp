#include "HAClient.h"
#include "hal/Wifi.h"
#include "services/ConfigStore.h"
#include "util/Logger.h"
#include <ArduinoJson.h>

namespace paperos {

void HAClient::begin() {
    url_ = cfg_.haUrl();
    http_.setBearerToken(cfg_.haToken());
    http_.setTimeoutMs(5000);
}

bool HAClient::ensureWifi() {
    if (wifi_.isConnected()) return true;
    auto r = wifi_.connect(cfg_.wifiSsid(), cfg_.wifiPass(), 8000);
    return r == WifiResult::Ok;
}

void HAClient::releaseWifi() { wifi_.disconnect(); }

bool HAClient::getState(const std::string& entity_id, EntityState& out) {
    if (url_.empty()) return false;
    std::string u = url_ + "/api/states/" + entity_id;
    auto resp = http_.get(u);
    if (!resp.ok()) { LOG_WARN("ha", "GET %s -> %d", u.c_str(), resp.status_code); return false; }
    JsonDocument doc;
    if (deserializeJson(doc, resp.body)) return false;
    out.entity_id = entity_id;
    out.state = doc["state"] | "";
    out.unit  = doc["attributes"]["unit_of_measurement"] | "";
    return true;
}

bool HAClient::ping(int& status, std::string& error) {
    status = 0;
    error.clear();
    if (url_.empty()) { error = "url-empty"; return false; }
    std::string u = url_ + "/api/";
    auto resp = http_.get(u);
    status = resp.status_code;
    error = resp.error;
    if (!resp.ok()) {
        LOG_WARN("ha", "ping %s -> status=%d error=%s",
                 u.c_str(), resp.status_code, resp.error.c_str());
        return false;
    }
    return true;
}

bool HAClient::callService(const std::string& domain, const std::string& service,
                           const std::string& entity_id) {
    if (url_.empty()) return false;
    std::string u = url_ + "/api/services/" + domain + "/" + service;
    std::string body = "{\"entity_id\":\"" + entity_id + "\"}";
    auto resp = http_.post(u, body);
    if (!resp.ok()) { LOG_WARN("ha", "POST %s -> %d", u.c_str(), resp.status_code); return false; }
    return true;
}

} // namespace paperos
