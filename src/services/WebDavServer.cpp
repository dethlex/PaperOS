#include "WebDavServer.h"
#include "hal/Sd.h"
#include "util/WebDavPath.h"
#include "util/WebDavXml.h"
#include "util/Logger.h"
#include <FS.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace paperos {

// --- file-static helpers ---

// Ответ только со статусом и пустым телом (для кодов, которых нет в httpd_err_code_t).
static esp_err_t sendSimple(httpd_req_t* r, const char* status) {
    httpd_resp_set_status(r, status);
    httpd_resp_send(r, nullptr, 0);
    return ESP_OK;
}

// Слить непрочитанное тело запроса (PROPFIND/LOCK шлют XML) — иначе keep-alive
// соединение получит мусор в следующем запросе.
static void drainBody(httpd_req_t* r) {
    char tmp[256];
    int remaining = r->content_len;
    while (remaining > 0) {
        int want = remaining < (int)sizeof(tmp) ? remaining : (int)sizeof(tmp);
        int n = httpd_req_recv(r, tmp, want);
        if (n <= 0) { if (n == HTTPD_SOCK_ERR_TIMEOUT) continue; break; }
        remaining -= n;
    }
}

bool WebDavServer::reqPath(httpd_req_t* r, std::string& sdPath, std::string& urlPath) {
    urlPath = r->uri;
    auto q = urlPath.find('?');
    if (q != std::string::npos) urlPath.resize(q);
    return mapUrlToSdPath(urlPath, sdPath);
}

// --- read handlers ---

esp_err_t WebDavServer::options(httpd_req_t* r) {
    drainBody(r);
    httpd_resp_set_hdr(r, "DAV", "1, 2");
    httpd_resp_set_hdr(r, "Allow",
        "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, MOVE, LOCK, UNLOCK");
    httpd_resp_set_hdr(r, "MS-Author-Via", "DAV");
    httpd_resp_send(r, nullptr, 0);   // httpd сам выставит Content-Length: 0 — свой не ставим
    return ESP_OK;
}

esp_err_t WebDavServer::propfind(httpd_req_t* r) {
    drainBody(r);
    std::string sdPath, urlPath;
    if (!reqPath(r, sdPath, urlPath)) return sendSimple(r, "400 Bad Request");

    bool isDir; uint32_t size, mtime;
    if (!sd_.stat(sdPath.c_str(), isDir, size, mtime)) return sendSimple(r, "404 Not Found");

    char depth[16] = {0};
    httpd_req_get_hdr_value_str(r, "Depth", depth, sizeof(depth));
    bool deep = (depth[0] != '0');   // дефолт (нет заголовка) → как "1"

    std::vector<DavEntry> children;
    if (isDir && deep) {
        for (auto& de : sd_.listDir(sdPath.c_str()))
            children.push_back({de.name, de.isDir, de.size, de.mtime});
    }

    std::string href = urlPath.empty() ? "/" : urlPath;
    std::string xml  = buildPropfindXml(href, isDir, children, isDir ? 0 : size, mtime);

    httpd_resp_set_status(r, "207 Multi-Status");
    httpd_resp_set_type(r, "application/xml; charset=\"utf-8\"");
    httpd_resp_send(r, xml.c_str(), xml.size());
    return ESP_OK;
}

esp_err_t WebDavServer::get(httpd_req_t* r, bool headOnly) {
    drainBody(r);
    std::string sdPath, urlPath;
    if (!reqPath(r, sdPath, urlPath)) return sendSimple(r, "400 Bad Request");

    bool isDir; uint32_t size, mtime;
    if (!sd_.stat(sdPath.c_str(), isDir, size, mtime)) return sendSimple(r, "404 Not Found");

    if (isDir) {   // на папку листинг не отдаём
        httpd_resp_set_type(r, "text/plain");
        const char* msg = "Collection";
        httpd_resp_send(r, headOnly ? nullptr : msg, headOnly ? 0 : strlen(msg));
        return ESP_OK;
    }

    if (headOnly) {
        // httpd_resp_send(NULL,0) сам выставляет Content-Length: 0; свой размер не
        // ставим, иначе вышло бы два конфликтующих Content-Length (нарушение RFC).
        // Клиенты берут размер из PROPFIND; chunked-GET всё равно без Content-Length.
        httpd_resp_set_type(r, "application/octet-stream");
        httpd_resp_send(r, nullptr, 0);
        return ESP_OK;
    }

    File f = sd_.openRead(sdPath.c_str());
    if (!f) return sendSimple(r, "404 Not Found");
    httpd_resp_set_type(r, "application/octet-stream");

    const size_t BUF = 2048;
    std::vector<uint8_t> buf(BUF);
    int n;
    while ((n = f.read(buf.data(), BUF)) > 0) {
        if (httpd_resp_send_chunk(r, (const char*)buf.data(), n) != ESP_OK) {
            f.close();
            return ESP_FAIL;
        }
    }
    f.close();
    httpd_resp_send_chunk(r, nullptr, 0);   // финальный пустой chunk → конец
    return ESP_OK;
}

// --- write handlers ---

esp_err_t WebDavServer::put(httpd_req_t* r) {
    std::string sdPath, urlPath;
    if (!reqPath(r, sdPath, urlPath)) return sendSimple(r, "400 Bad Request");

    bool existed = sd_.exists(sdPath.c_str());
    File f = sd_.openWrite(sdPath.c_str());
    if (!f) return sendSimple(r, "500 Internal Server Error");

    // Тело читаем по Content-Length. ВАЖНО: chunked-тело (Transfer-Encoding: chunked,
    // content_len==0) esp_http_server не поддерживает — цикл не выполнится и файл
    // останется пустым. Это и есть причина, по которой запись из macOS Finder
    // (всегда chunked) не работает; клиенты с Content-Length (curl -T/Cyberduck/
    // rclone/Windows) шлют длину и пишут корректно. Подробно — spec §12.
    const size_t BUF = 2048;
    std::vector<char> buf(BUF);
    int remaining = r->content_len;
    bool ok = true;
    while (remaining > 0) {
        int want = remaining < (int)BUF ? remaining : (int)BUF;
        int n = httpd_req_recv(r, buf.data(), want);
        if (n <= 0) {
            if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ok = false; break;
        }
        if (f.write((const uint8_t*)buf.data(), n) != (size_t)n) { ok = false; break; }
        remaining -= n;
    }
    f.close();
    if (!ok) return sendSimple(r, "500 Internal Server Error");
    return sendSimple(r, existed ? "204 No Content" : "201 Created");
}

esp_err_t WebDavServer::del(httpd_req_t* r) {
    drainBody(r);
    std::string sdPath, urlPath;
    if (!reqPath(r, sdPath, urlPath)) return sendSimple(r, "400 Bad Request");
    bool isDir; uint32_t size, mtime;
    if (!sd_.stat(sdPath.c_str(), isDir, size, mtime)) return sendSimple(r, "404 Not Found");
    bool ok = isDir ? sd_.removeTree(sdPath.c_str()) : sd_.removePath(sdPath.c_str());
    return sendSimple(r, ok ? "204 No Content" : "500 Internal Server Error");
}

esp_err_t WebDavServer::mkcol(httpd_req_t* r) {
    drainBody(r);
    std::string sdPath, urlPath;
    if (!reqPath(r, sdPath, urlPath)) return sendSimple(r, "400 Bad Request");
    if (sd_.exists(sdPath.c_str())) return sendSimple(r, "405 Method Not Allowed");
    return sendSimple(r, sd_.makeDir(sdPath.c_str()) ? "201 Created" : "409 Conflict");
}

esp_err_t WebDavServer::move(httpd_req_t* r) {
    drainBody(r);
    std::string srcPath, srcUrl;
    if (!reqPath(r, srcPath, srcUrl)) return sendSimple(r, "400 Bad Request");

    size_t dlen = httpd_req_get_hdr_value_len(r, "Destination");
    if (dlen == 0) return sendSimple(r, "400 Bad Request");
    std::vector<char> dbuf(dlen + 1);
    httpd_req_get_hdr_value_str(r, "Destination", dbuf.data(), dlen + 1);
    std::string dest(dbuf.data());

    // Destination может быть полным URL ("http://host/path") — берём путь.
    auto scheme = dest.find("://");
    if (scheme != std::string::npos) {
        auto slash = dest.find('/', scheme + 3);
        dest = (slash == std::string::npos) ? "/" : dest.substr(slash);
    }
    std::string destPath;
    if (!mapUrlToSdPath(dest, destPath)) return sendSimple(r, "403 Forbidden");
    // Self-move (src == dest): RFC 4918 §9.9.4 → 403. КРИТИЧНО: без этой проверки
    // ветка overwrite ниже удалила бы сам файл, а rename затем упал бы → потеря данных.
    if (srcPath == destPath) return sendSimple(r, "403 Forbidden");
    if (!sd_.exists(srcPath.c_str())) return sendSimple(r, "404 Not Found");

    bool destExisted = sd_.exists(destPath.c_str());
    // overwrite: rename не перезапишет существующий. removeTree снимает и файл, и
    // непустую папку (removePath/rmdir на непустой папке провалился бы → 500).
    if (destExisted) sd_.removeTree(destPath.c_str());
    if (!sd_.renamePath(srcPath.c_str(), destPath.c_str())) return sendSimple(r, "500 Internal Server Error");
    return sendSimple(r, destExisted ? "204 No Content" : "201 Created");
}

esp_err_t WebDavServer::lock(httpd_req_t* r) {
    drainBody(r);
    char token[64];
    snprintf(token, sizeof(token), "opaquelocktoken:paperos-%u", (unsigned)(++lock_counter_));
    char hdr[80];
    snprintf(hdr, sizeof(hdr), "<%s>", token);     // RFC: Lock-Token в угловых скобках
    std::string xml = buildLockXml(token, 3600);
    httpd_resp_set_hdr(r, "Lock-Token", hdr);      // hdr живёт до return (после send) — ок
    httpd_resp_set_type(r, "application/xml; charset=\"utf-8\"");
    httpd_resp_send(r, xml.c_str(), xml.size());
    return ESP_OK;
}

esp_err_t WebDavServer::unlock(httpd_req_t* r) {
    drainBody(r);
    return sendSimple(r, "204 No Content");
}

esp_err_t WebDavServer::proppatch(httpd_req_t* r) {
    // Свойства не храним — отвечаем «всё установлено» (207). Без этого macOS Finder
    // (шлёт PROPPATCH сразу после PUT) валит копирование ошибкой -36.
    drainBody(r);
    std::string sdPath, urlPath;
    if (!reqPath(r, sdPath, urlPath)) return sendSimple(r, "400 Bad Request");
    std::string href = urlPath.empty() ? "/" : urlPath;
    std::string xml  = buildProppatchXml(href);
    httpd_resp_set_status(r, "207 Multi-Status");
    httpd_resp_set_type(r, "application/xml; charset=\"utf-8\"");
    httpd_resp_send(r, xml.c_str(), xml.size());
    return ESP_OK;
}

// --- trampolines ---
esp_err_t WebDavServer::hOptions(httpd_req_t* r)  { return static_cast<WebDavServer*>(r->user_ctx)->options(r); }
esp_err_t WebDavServer::hPropfind(httpd_req_t* r) { return static_cast<WebDavServer*>(r->user_ctx)->propfind(r); }
esp_err_t WebDavServer::hGet(httpd_req_t* r)      { return static_cast<WebDavServer*>(r->user_ctx)->get(r, false); }
esp_err_t WebDavServer::hHead(httpd_req_t* r)     { return static_cast<WebDavServer*>(r->user_ctx)->get(r, true); }
esp_err_t WebDavServer::hPut(httpd_req_t* r)    { return static_cast<WebDavServer*>(r->user_ctx)->put(r); }
esp_err_t WebDavServer::hDelete(httpd_req_t* r) { return static_cast<WebDavServer*>(r->user_ctx)->del(r); }
esp_err_t WebDavServer::hMkcol(httpd_req_t* r)  { return static_cast<WebDavServer*>(r->user_ctx)->mkcol(r); }
esp_err_t WebDavServer::hMove(httpd_req_t* r)   { return static_cast<WebDavServer*>(r->user_ctx)->move(r); }
esp_err_t WebDavServer::hLock(httpd_req_t* r)   { return static_cast<WebDavServer*>(r->user_ctx)->lock(r); }
esp_err_t WebDavServer::hUnlock(httpd_req_t* r) { return static_cast<WebDavServer*>(r->user_ctx)->unlock(r); }
esp_err_t WebDavServer::hProppatch(httpd_req_t* r) { return static_cast<WebDavServer*>(r->user_ctx)->proppatch(r); }

// --- lifecycle ---
bool WebDavServer::start(uint16_t port) {
    if (handle_) return true;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = port;
    cfg.max_uri_handlers = 16;
    cfg.stack_size       = 8192;
    cfg.uri_match_fn     = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;
    if (httpd_start(&handle_, &cfg) != ESP_OK) { handle_ = nullptr; return false; }

    auto reg = [&](httpd_method_t m, esp_err_t (*h)(httpd_req_t*)) {
        httpd_uri_t u = {};
        u.uri      = "/*";
        u.method   = m;
        u.handler  = h;
        u.user_ctx = this;
        httpd_register_uri_handler(handle_, &u);
    };
    reg(HTTP_OPTIONS,  hOptions);
    reg(HTTP_PROPFIND, hPropfind);
    reg(HTTP_GET,      hGet);
    reg(HTTP_HEAD,     hHead);
    reg(HTTP_PUT,    hPut);
    reg(HTTP_DELETE, hDelete);
    reg(HTTP_MKCOL,  hMkcol);
    reg(HTTP_MOVE,   hMove);
    reg(HTTP_LOCK,   hLock);
    reg(HTTP_UNLOCK, hUnlock);
    reg(HTTP_PROPPATCH, hProppatch);

    LOG_INFO("webdav", "started on :%u", (unsigned)port);
    return true;
}

void WebDavServer::stop() {
    if (handle_) { httpd_stop(handle_); handle_ = nullptr; LOG_INFO("webdav", "stopped"); }
}

} // namespace paperos
