#include "MoonrakerClient.h"
#include "hal/Wifi.h"
#include "services/ConfigStore.h"
#include "util/Logger.h"

namespace paperos {

void MoonrakerClient::begin() {
    url_ = cfg_.printerUrl();
    while (!url_.empty() && url_.back() == '/') url_.pop_back();   // clean concat

    lightName_ = cfg_.printerLightName();
    if (lightName_.empty()) lightName_ = "LED_Light";

    lightOn_  = cfg_.printerLightOn();
    lightOff_ = cfg_.printerLightOff();
    if (lightOn_.empty())  lightOn_  = "SET_LED LED=" + lightName_ + " WHITE=1";
    if (lightOff_.empty()) lightOff_ = "SET_LED LED=" + lightName_ + " WHITE=0";

    http_.setApiKey(cfg_.printerApiKey());
    http_.setTimeoutMs(5000);
    lightObject_.clear();

    powerDevice_   = cfg_.printerPowerDevice();
    preheatNozzle_ = cfg_.printerPreheatNozzle();
    preheatBed_    = cfg_.printerPreheatBed();

    meta_cache_file_.clear();          // config/URL may have changed
    meta_cache_ = FileMeta{};
}

bool MoonrakerClient::ensureWifi() {
    if (wifi_.isConnected()) return true;
    return wifi_.connect(cfg_.wifiSsid(), cfg_.wifiPass(), 8000) == WifiResult::Ok;
}

void MoonrakerClient::releaseWifi() { wifi_.disconnect(); }

bool MoonrakerClient::discoverLightObject() {
    if (url_.empty()) return false;
    auto resp = http_.get(url_ + "/printer/objects/list");
    if (!resp.ok()) { LOG_WARN("printer", "objects/list -> %d", resp.status_code); return false; }
    lightObject_ = findLightObject(resp.body, lightName_);
    return !lightObject_.empty();
}

bool MoonrakerClient::discoverPowerDevice() {
    if (url_.empty()) return false;
    if (!powerDevice_.empty()) return true;        // config override wins
    auto resp = http_.get(url_ + "/machine/device_power/devices");
    if (!resp.ok()) { LOG_WARN("printer", "device_power/devices -> %d", resp.status_code); return false; }
    powerDevice_ = parseFirstPowerDevice(resp.body);
    return !powerDevice_.empty();
}

bool MoonrakerClient::pollStatus(PrinterStatus& out) {
    if (url_.empty()) return false;
    std::string body =
        "{\"objects\":{"
        "\"webhooks\":[\"state\",\"state_message\"],"
        "\"print_stats\":[\"state\",\"filename\",\"print_duration\",\"info\"],"
        "\"toolhead\":[\"homed_axes\"],"
        "\"display_status\":[\"progress\"],"
        "\"virtual_sdcard\":[\"progress\",\"file_position\"],"
        "\"extruder\":[\"temperature\",\"target\"],"
        "\"heater_bed\":[\"temperature\",\"target\"]";
    if (!lightObject_.empty())
        body += ",\"" + lightObject_ + "\":[\"color_data\"]";
    body += "}}";

    auto resp = http_.post(url_ + "/printer/objects/query", body);
    if (!resp.ok()) { LOG_WARN("printer", "objects/query -> %d", resp.status_code); return false; }
    return parsePrinterStatus(resp.body, lightObject_, out);
}

bool MoonrakerClient::setLight(bool on) {
    if (url_.empty()) return false;
    const std::string& script = on ? lightOn_ : lightOff_;
    std::string body = "{\"script\":\"" + jsonEscape(script) + "\"}";
    auto resp = http_.post(url_ + "/printer/gcode/script", body);
    if (!resp.ok()) { LOG_WARN("printer", "gcode/script -> %d", resp.status_code); return false; }
    return true;
}

namespace {
bool postOk(HttpClient& http, const std::string& url, const std::string& body) {
    auto resp = http.post(url, body);
    if (!resp.ok()) { LOG_WARN("printer", "POST %s -> %d", url.c_str(), resp.status_code); return false; }
    return true;
}
} // namespace

bool MoonrakerClient::setPower(bool on) {
    if (url_.empty() || powerDevice_.empty()) return false;
    return postOk(http_, buildPowerUrl(url_, powerDevice_, on), "");
}

PowerState MoonrakerClient::fetchPower() {
    if (url_.empty() || powerDevice_.empty()) return PowerState::Unknown;
    auto resp = http_.get(buildPowerStatusUrl(url_, powerDevice_));
    if (!resp.ok()) { LOG_WARN("printer", "device_power -> %d", resp.status_code); return PowerState::Unknown; }
    return parsePowerState(resp.body, powerDevice_);
}

bool MoonrakerClient::jog(char axis, int deltaMm, int feed) {
    if (url_.empty()) return false;
    std::string body = "{\"script\":\"" + jsonEscape(buildJogScript(axis, deltaMm, feed)) + "\"}";
    return postOk(http_, url_ + "/printer/gcode/script", body);
}

bool MoonrakerClient::home() {
    if (url_.empty()) return false;
    std::string body = "{\"script\":\"" + jsonEscape(std::string("G28")) + "\"}";
    return postOk(http_, url_ + "/printer/gcode/script", body);
}

bool MoonrakerClient::firmwareRestart() {
    if (url_.empty()) return false;
    return postOk(http_, url_ + "/printer/firmware_restart", "");
}

bool MoonrakerClient::preheat(int nozzle, int bed) {
    if (url_.empty()) return false;
    std::string body = "{\"script\":\"" + jsonEscape(buildPreheatScript(nozzle, bed)) + "\"}";
    return postOk(http_, url_ + "/printer/gcode/script", body);
}

bool MoonrakerClient::cooldown() {
    if (url_.empty()) return false;
    std::string body = "{\"script\":\"" + jsonEscape(std::string("TURN_OFF_HEATERS")) + "\"}";
    return postOk(http_, url_ + "/printer/gcode/script", body);
}

bool MoonrakerClient::startPrint(const std::string& filename) {
    if (url_.empty() || filename.empty()) return false;
    return postOk(http_, buildPrintStartUrl(url_, filename), "");
}

bool MoonrakerClient::cancelPrint() {
    if (url_.empty()) return false;
    return postOk(http_, url_ + "/printer/print/cancel", "");
}

bool MoonrakerClient::pausePrint() {
    if (url_.empty()) return false;
    return postOk(http_, url_ + "/printer/print/pause", "");
}

bool MoonrakerClient::resumePrint() {
    if (url_.empty()) return false;
    return postOk(http_, url_ + "/printer/print/resume", "");
}

bool MoonrakerClient::fetchHistory(std::vector<HistoryEntry>& out, size_t maxItems) {
    if (url_.empty()) return false;
    auto resp = http_.get(url_ + "/server/history/list?limit=10&order=desc");
    if (!resp.ok()) { LOG_WARN("printer", "history/list -> %d", resp.status_code); return false; }
    out = parseHistory(resp.body, maxItems);
    return true;
}

bool MoonrakerClient::fetchFileMeta(const std::string& filename, FileMeta& out) {
    if (!filename.empty() && filename == meta_cache_file_) {
        out = meta_cache_;
        return true;
    }
    out = FileMeta{};
    if (url_.empty() || filename.empty()) return false;
    auto meta = http_.get(buildMetadataUrl(url_, filename));
    if (!meta.ok()) { LOG_WARN("printer", "files/metadata -> %d", meta.status_code); return false; }
    out.estimatedS = parseSlicerEstimate(meta.body);
    if (!parseGcodeByteRange(meta.body, out.gcodeStart, out.gcodeEnd)) {
        out.gcodeStart = out.gcodeEnd = 0;
    }
    std::string rel = parseThumbnailPath(meta.body);
    if (!rel.empty()) {
        if (!http_.getBinary(buildThumbnailUrl(url_, filename, rel), out.thumbPng, kThumbMaxBytes)) {
            LOG_WARN("printer", "thumbnail fetch failed");
        }
    }
    meta_cache_file_ = filename;
    meta_cache_ = out;
    return true;
}

} // namespace paperos