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
    if (!api_key_.empty()) http_.addHeader("X-Api-Key", api_key_.c_str());

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

bool HttpClient::getBinary(const std::string& url, std::vector<uint8_t>& out, size_t maxLen) {
    out.clear();
    if (WiFi.status() != WL_CONNECTED) return false;

    const bool is_https = url.size() >= 8 && url.compare(0, 8, "https://") == 0;
    http_.setReuse(true);
    http_.setTimeout(timeout_ms_);
    bool ok_begin;
    if (is_https) {
        if (!secure_ready_) { secure_.setInsecure(); secure_ready_ = true; }
        ok_begin = http_.begin(secure_, url.c_str());
    } else {
        ok_begin = http_.begin(url.c_str());
    }
    if (!ok_begin) return false;
    if (!bearer_.empty()) http_.addHeader("Authorization", ("Bearer " + bearer_).c_str());
    if (!api_key_.empty()) http_.addHeader("X-Api-Key", api_key_.c_str());

    int code = http_.GET();
    if (code < 200 || code >= 300) { http_.end(); return false; }

    int remaining = http_.getSize();
    // getStreamPtr() is the RAW socket: HTTPClient's chunked decoder is
    // bypassed, so an unknown length (-1, chunked) would leak chunk framing
    // into the binary payload. Reject it — Moonraker serves files with
    // Content-Length; anything else is not worth silently corrupting.
    if (remaining <= 0) { http_.end(); return false; }
    WiFiClient* stream = http_.getStreamPtr();
    if (!stream) { http_.end(); return false; }
    uint32_t deadline = millis() + timeout_ms_;
    uint8_t buf[512];
    while (http_.connected() && remaining > 0) {
        size_t avail = stream->available();
        if (avail) {
            size_t n = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
            if (out.size() + n > maxLen) { http_.end(); out.clear(); return false; }
            out.insert(out.end(), buf, buf + n);
            remaining -= static_cast<int>(n);
            if (remaining == 0) break;
            deadline = millis() + timeout_ms_;     // progress resets the timeout
        } else {
            if (static_cast<int32_t>(millis() - deadline) >= 0) break;
            delay(1);
        }
    }
    http_.end();
    return remaining == 0;                         // full declared body received
}

} // namespace paperos
