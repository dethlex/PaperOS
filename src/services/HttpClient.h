#pragma once
#include <string>
#include <stdint.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace paperos {

struct HttpResponse {
    int status_code = 0;        // 0 = transport error
    std::string body;
    std::string error;          // human-readable error if status_code == 0
    bool ok() const { return status_code >= 200 && status_code < 300; }
};

// Thin wrapper over Arduino HTTPClient with bearer auth + connection reuse.
//
// The HTTPClient and WiFiClientSecure are PERSISTENT members (not per-request
// locals) and setReuse(true) is enabled, so repeated requests to the same host
// keep the TCP/TLS connection alive. Only the first request pays the ~1-2s TLS
// handshake; subsequent ones reuse the open socket (~tens of ms). This is what
// makes the HA dashboard's per-entity polling fast enough not to stall the UI.
class HttpClient {
public:
    void setBearerToken(const std::string& token) { bearer_ = token; }
    void setTimeoutMs(uint32_t ms) { timeout_ms_ = ms; }

    HttpResponse get(const std::string& url);
    HttpResponse post(const std::string& url, const std::string& json_body);

private:
    HttpResponse doRequest(const std::string& url, const std::string& body,
                           const char* method);

    std::string bearer_;
    uint32_t timeout_ms_ = 5000;

    WiFiClientSecure secure_;     // persistent → TLS session reused across calls
    bool secure_ready_ = false;   // setInsecure() applied once
    HTTPClient http_;             // persistent → HTTP keep-alive
};

} // namespace paperos
