#pragma once
#include <esp_http_server.h>
#include <cstdint>
#include <string>

namespace paperos {

class Sd;

class WebDavServer {
public:
    explicit WebDavServer(Sd& sd) : sd_(sd) {}
    bool start(uint16_t port = 80);   // httpd_start + регистрация хэндлеров
    void stop();                      // httpd_stop (идемпотентно)
    bool running() const { return handle_ != nullptr; }

private:
    Sd&            sd_;
    httpd_handle_t handle_ = nullptr;
    uint32_t       lock_counter_ = 0;

    // URL запроса (req->uri без query) → SD-путь под /paperos. false → traversal/битый.
    bool reqPath(httpd_req_t* r, std::string& sdPath, std::string& urlPath);

    // Трамплины: восстанавливают WebDavServer* из req->user_ctx.
    static esp_err_t hOptions(httpd_req_t*);
    static esp_err_t hPropfind(httpd_req_t*);
    static esp_err_t hGet(httpd_req_t*);
    static esp_err_t hHead(httpd_req_t*);
    static esp_err_t hPut(httpd_req_t*);
    static esp_err_t hDelete(httpd_req_t*);
    static esp_err_t hMkcol(httpd_req_t*);
    static esp_err_t hMove(httpd_req_t*);
    static esp_err_t hLock(httpd_req_t*);
    static esp_err_t hUnlock(httpd_req_t*);
    static esp_err_t hProppatch(httpd_req_t*);

    // Инстанс-реализации.
    esp_err_t options(httpd_req_t*);
    esp_err_t propfind(httpd_req_t*);
    esp_err_t get(httpd_req_t*, bool headOnly);
    esp_err_t put(httpd_req_t*);
    esp_err_t del(httpd_req_t*);
    esp_err_t mkcol(httpd_req_t*);
    esp_err_t move(httpd_req_t*);
    esp_err_t lock(httpd_req_t*);
    esp_err_t unlock(httpd_req_t*);
    esp_err_t proppatch(httpd_req_t*);
};

} // namespace paperos
