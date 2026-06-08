#pragma once
#include "services/HttpClient.h"
#include <string>

namespace paperos {

class ConfigStore;
class Wifi;

struct EntityState {
    std::string entity_id;
    std::string state;          // "on"/"off"/number-as-string
    std::string unit;
};

class HAClient {
public:
    HAClient(Wifi& w, ConfigStore& c) : wifi_(w), cfg_(c) {}

    void begin();    // reads url/token from ConfigStore

    bool ensureWifi();
    void releaseWifi();

    // GET /api/states/<entity_id>. Returns false on transport/HTTP errors.
    bool getState(const std::string& entity_id, EntityState& out);

    // GET /api/  — minimal "is HA reachable + token valid" check.
    // On success returns true; on failure populates `status` (0 = transport
    // error before HTTP, else HTTP status) and `error` (transport reason).
    bool ping(int& status, std::string& error);

    // POST /api/services/<domain>/<service> with body {"entity_id": "..."}.
    bool callService(const std::string& domain, const std::string& service,
                     const std::string& entity_id);
private:
    Wifi& wifi_;
    ConfigStore& cfg_;
    HttpClient http_;
    std::string url_;
};

} // namespace paperos
