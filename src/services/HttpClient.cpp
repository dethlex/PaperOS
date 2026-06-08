#include "HttpClient.h"
#include <WiFi.h>
#include <string.h>
#include "util/Logger.h"

namespace paperos {

// Uses the persistent http_ / secure_ members so the TCP/TLS connection is
// reused across calls (setReuse(true)). begin() on the same host reuses the
// open socket; only the first call does the TLS handshake. begin() also clears
// per-request headers internally, so re-adding Authorization/Content-Type each
// time does not duplicate them.
HttpResponse HttpClient::doRequest(const std::string& url, const std::string& body,
                                   const char* method) {
    HttpResponse r;
    if (WiFi.status() != WL_CONNECTED) { r.error = "wifi-down"; return r; }

    const bool is_https = url.size() >= 8 && url.compare(0, 8, "https://") == 0;
    http_.setReuse(true);
    http_.setTimeout(timeout_ms_);

    bool ok_begin;
    if (is_https) {
        // Accept self-signed / untrusted certs — PaperOS is a home-LAN device.
        // Swap setInsecure() for setCACert(rootCA) if strict validation is
        // ever needed. Applied once; the TLS session is then reused.
        if (!secure_ready_) { secure_.setInsecure(); secure_ready_ = true; }
        ok_begin = http_.begin(secure_, url.c_str());
    } else {
        ok_begin = http_.begin(url.c_str());
    }
    if (!ok_begin) { r.error = "begin-failed"; return r; }

    if (!bearer_.empty()) http_.addHeader("Authorization", ("Bearer " + bearer_).c_str());
    http_.addHeader("Content-Type", "application/json");

    int code = (strcmp(method, "POST") == 0)
        ? http_.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body.data())), body.size())
        : http_.GET();
    r.status_code = code;
    if (code > 0) r.body = http_.getString().c_str();
    else          r.error = HTTPClient::errorToString(code).c_str();

    // With setReuse(true), end() keeps the underlying connection alive for the
    // next begin() instead of tearing down the socket.
    http_.end();
    return r;
}

HttpResponse HttpClient::get(const std::string& url) {
    return doRequest(url, {}, "GET");
}

HttpResponse HttpClient::post(const std::string& url, const std::string& body) {
    return doRequest(url, body, "POST");
}

} // namespace paperos
