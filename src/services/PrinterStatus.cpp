#include "PrinterStatus.h"
#include <ArduinoJson.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cctype>

namespace paperos {

// ---- Task 1: etaRemainingSec ----

uint32_t etaRemainingSec(uint32_t printDurationS, float progress) {
    if (progress <= 0.f || progress >= 1.f) return 0;
    float total = static_cast<float>(printDurationS) / progress;
    float rem = total - static_cast<float>(printDurationS);
    if (rem < 0.f) rem = 0.f;
    return static_cast<uint32_t>(rem + 0.5f);
}

// ---- Task 2: parsePrinterStatus helpers ----

namespace {
PrinterStatus::Klippy klippyFrom(const char* s) {
    if (!s) return PrinterStatus::Klippy::Unknown;
    if (!std::strcmp(s, "ready"))    return PrinterStatus::Klippy::Ready;
    if (!std::strcmp(s, "startup"))  return PrinterStatus::Klippy::Startup;
    if (!std::strcmp(s, "shutdown")) return PrinterStatus::Klippy::Shutdown;
    if (!std::strcmp(s, "error"))    return PrinterStatus::Klippy::Error;
    return PrinterStatus::Klippy::Unknown;
}
PrinterStatus::PrintSt printStFrom(const char* s) {
    if (!s) return PrinterStatus::PrintSt::Unknown;
    if (!std::strcmp(s, "standby"))   return PrinterStatus::PrintSt::Standby;
    if (!std::strcmp(s, "printing"))  return PrinterStatus::PrintSt::Printing;
    if (!std::strcmp(s, "paused"))    return PrinterStatus::PrintSt::Paused;
    if (!std::strcmp(s, "complete"))  return PrinterStatus::PrintSt::Complete;
    if (!std::strcmp(s, "cancelled")) return PrinterStatus::PrintSt::Cancelled;
    if (!std::strcmp(s, "error"))     return PrinterStatus::PrintSt::Error;
    return PrinterStatus::PrintSt::Unknown;
}
} // namespace

// ---- Task 2: parsePrinterStatus ----

bool parsePrinterStatus(const std::string& json, const std::string& lightObject,
                        PrinterStatus& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;
    // Moonraker's HTTP REST API wraps payloads in {"result": {...}}; the
    // WebSocket/JSON-RPC form returns the bare object. Accept both.
    JsonObjectConst root = doc["result"].as<JsonObjectConst>();
    if (root.isNull()) root = doc.as<JsonObjectConst>();
    JsonObjectConst st = root["status"].as<JsonObjectConst>();

    PrinterStatus s;

    JsonObjectConst w = st["webhooks"];
    s.klippy = klippyFrom(w["state"] | static_cast<const char*>(nullptr));
    s.klippyMessage = w["state_message"] | "";

    JsonObjectConst ps = st["print_stats"];
    s.state = printStFrom(ps["state"] | static_cast<const char*>(nullptr));
    s.filename = ps["filename"] | "";
    s.printDurationS = static_cast<uint32_t>(ps["print_duration"] | 0.0f);
    JsonObjectConst info = ps["info"];
    JsonVariantConst cl = info["current_layer"];
    JsonVariantConst tl = info["total_layer"];
    s.currentLayer = cl.isNull() ? -1 : cl.as<int>();
    s.totalLayer   = tl.isNull() ? -1 : tl.as<int>();

    JsonVariantConst dp = st["display_status"]["progress"];
    JsonVariantConst vp = st["virtual_sdcard"]["progress"];
    float p = 0.f;
    // display_status.progress (M73) can be STALE from the previous print until
    // the new file's first M73 runs (e.g. during heat-up). print_duration is 0
    // until actual printing starts — trust M73 only once printing has begun.
    if (!dp.isNull() && dp.as<float>() > 0.f && s.printDurationS > 0)
        p = dp.as<float>();                                   // M73 progress, when meaningful
    else if (!vp.isNull())
        p = vp.as<float>();                                   // else file position
    if (p < 0.f) p = 0.f;
    if (p > 1.f) p = 1.f;
    s.progress = p;

    JsonVariantConst fp = st["virtual_sdcard"]["file_position"];
    s.filePosition = fp.isNull() ? 0 : fp.as<uint32_t>();

    s.hotendTemp   = st["extruder"]["temperature"] | 0.0f;
    s.hotendTarget = st["extruder"]["target"] | 0.0f;
    s.bedTemp      = st["heater_bed"]["temperature"] | 0.0f;
    s.bedTarget    = st["heater_bed"]["target"] | 0.0f;

    s.homedAxes = st["toolhead"]["homed_axes"] | "";

    if (!lightObject.empty()) {
        JsonArrayConst cd = st[lightObject]["color_data"][0];
        if (!cd.isNull()) {
            bool on = false;
            for (JsonVariantConst ch : cd) {
                if (ch.as<float>() > 0.f) { on = true; break; }
            }
            s.lightOn = on;
            s.lightKnown = true;
        }
    }

    out = s;
    return true;
}

// ---- Task 3: sameForDisplay ----

namespace {
int pct(float p) { return static_cast<int>(p * 100.f + 0.5f); }
int ri(float t)  { return static_cast<int>(t < 0 ? t - 0.5f : t + 0.5f); }
} // namespace

bool sameForDisplay(const PrinterStatus& a, const PrinterStatus& b) {
    return a.klippy == b.klippy &&
           a.klippyMessage == b.klippyMessage &&
           a.state == b.state &&
           a.filename == b.filename &&
           a.homedAxes == b.homedAxes &&
           pct(a.progress) == pct(b.progress) &&
           ri(a.hotendTemp) == ri(b.hotendTemp) &&
           ri(a.hotendTarget) == ri(b.hotendTarget) &&
           ri(a.bedTemp) == ri(b.bedTemp) &&
           ri(a.bedTarget) == ri(b.bedTarget) &&
           a.currentLayer == b.currentLayer &&
           a.totalLayer == b.totalLayer &&
           a.lightOn == b.lightOn &&
           a.lightKnown == b.lightKnown &&
           (a.printDurationS / 60) == (b.printDurationS / 60) &&
           (etaRemainingSec(a.printDurationS, a.progress) / 60) ==
               (etaRemainingSec(b.printDurationS, b.progress) / 60);
}

// ---- Task 4: findLightObject ----

std::string findLightObject(const std::string& listJson, const std::string& lightName) {
    if (lightName.empty()) return "";
    JsonDocument doc;
    if (deserializeJson(doc, listJson)) return "";
    // Same {"result": {...}} REST envelope as parsePrinterStatus; accept bare too.
    JsonObjectConst root = doc["result"].as<JsonObjectConst>();
    if (root.isNull()) root = doc.as<JsonObjectConst>();
    JsonArrayConst arr = root["objects"].as<JsonArrayConst>();
    if (arr.isNull()) return "";

    const std::string suffix = " " + lightName;
    std::string best;
    int bestRank = 0;
    for (JsonVariantConst v : arr) {
        const char* name = v.as<const char*>();
        if (!name) continue;
        std::string n = name;
        if (n.size() <= suffix.size()) continue;
        if (n.compare(n.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        int rank = 1;
        if (n.rfind("led ", 0) == 0)            rank = 3;
        else if (n.rfind("neopixel ", 0) == 0)  rank = 2;
        if (rank > bestRank) { bestRank = rank; best = n; }
    }
    return best;
}

// ---- Task 5: PrinterCache ----

namespace {
int16_t r16(float t) { return static_cast<int16_t>(t < 0 ? t - 0.5f : t + 0.5f); }
} // namespace

PrinterCache toCache(const PrinterStatus& s) {
    PrinterCache c{};
    c.version = 1;
    c.klippy  = static_cast<uint8_t>(s.klippy);
    c.state   = static_cast<uint8_t>(s.state);
    c.flags   = static_cast<uint8_t>((s.lightOn ? 1 : 0) | (s.lightKnown ? 2 : 0));
    c.progress = s.progress;
    c.printDurationS = s.printDurationS;
    c.currentLayer = static_cast<int16_t>(s.currentLayer);
    c.totalLayer   = static_cast<int16_t>(s.totalLayer);
    c.hotendTemp = r16(s.hotendTemp); c.hotendTarget = r16(s.hotendTarget);
    c.bedTemp    = r16(s.bedTemp);    c.bedTarget    = r16(s.bedTarget);
    return c;
}

PrinterStatus fromCache(const PrinterCache& c) {
    PrinterStatus s;
    if (c.version != 1) return s;
    s.klippy = static_cast<PrinterStatus::Klippy>(c.klippy);
    s.state  = static_cast<PrinterStatus::PrintSt>(c.state);
    s.lightOn    = (c.flags & 1) != 0;
    s.lightKnown = (c.flags & 2) != 0;
    s.progress = c.progress;
    s.printDurationS = c.printDurationS;
    s.currentLayer = c.currentLayer;
    s.totalLayer   = c.totalLayer;
    s.hotendTemp = c.hotendTemp; s.hotendTarget = c.hotendTarget;
    s.bedTemp    = c.bedTemp;    s.bedTarget    = c.bedTarget;
    return s;
}

// ---- Task 4: parsePowerState ----

PowerState parsePowerState(const std::string& json, const std::string& device) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return PowerState::Unknown;
    JsonObjectConst root = doc["result"].as<JsonObjectConst>();
    if (root.isNull()) root = doc.as<JsonObjectConst>();
    const char* st = root[device.c_str()] | static_cast<const char*>(nullptr);
    if (!st) return PowerState::Unknown;
    if (!std::strcmp(st, "on"))  return PowerState::On;
    if (!std::strcmp(st, "off")) return PowerState::Off;
    return PowerState::Unknown;
}

// ---- Redesign: parseFirstPowerDevice ----

std::string parseFirstPowerDevice(const std::string& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return "";
    JsonObjectConst root = doc["result"].as<JsonObjectConst>();
    if (root.isNull()) root = doc.as<JsonObjectConst>();
    JsonArrayConst devs = root["devices"].as<JsonArrayConst>();
    if (devs.isNull()) return "";
    for (JsonObjectConst d : devs) {
        const char* name = d["device"] | static_cast<const char*>(nullptr);
        if (name && *name) return name;
    }
    return "";
}

// ---- Task 2: URL builders ----

std::string buildPowerUrl(const std::string& base, const std::string& device, bool on) {
    return base + "/machine/device_power/" + (on ? "on" : "off") + "?" + urlEncode(device);
}

std::string buildPowerStatusUrl(const std::string& base, const std::string& device) {
    return base + "/machine/device_power/device?device=" + urlEncode(device);
}

std::string buildPrintStartUrl(const std::string& base, const std::string& filename) {
    return base + "/printer/print/start?filename=" + urlEncode(filename);
}

// ---- Slicer estimate ----

uint32_t parseSlicerEstimate(const std::string& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return 0;
    JsonObjectConst root = doc["result"].as<JsonObjectConst>();
    if (root.isNull()) root = doc.as<JsonObjectConst>();
    float est = root["estimated_time"] | 0.0f;
    if (est < 0) est = 0;
    return static_cast<uint32_t>(est + 0.5f);
}

std::string buildMetadataUrl(const std::string& base, const std::string& filename) {
    return base + "/server/files/metadata?filename=" + urlEncode(filename);
}

// ---- File-relative progress (Fluidd-style byte range) ----

bool parseGcodeByteRange(const std::string& metadataJson, uint32_t& startB, uint32_t& endB) {
    JsonDocument doc;
    if (deserializeJson(doc, metadataJson)) return false;
    JsonObjectConst root = doc["result"].as<JsonObjectConst>();
    if (root.isNull()) root = doc.as<JsonObjectConst>();
    JsonVariantConst sb = root["gcode_start_byte"], eb = root["gcode_end_byte"];
    if (sb.isNull() || eb.isNull()) return false;
    startB = sb.as<uint32_t>();
    endB   = eb.as<uint32_t>();
    return endB > startB;
}

float fileRelativeProgress(uint32_t pos, uint32_t startB, uint32_t endB) {
    if (endB <= startB) return 0.f;
    if (pos <= startB) return 0.f;
    if (pos >= endB) return 1.f;
    return static_cast<float>(pos - startB) / static_cast<float>(endB - startB);
}

// ---- Redesign: thumbnail path/url ----

std::string parseThumbnailPath(const std::string& metadataJson) {
    JsonDocument doc;
    if (deserializeJson(doc, metadataJson)) return "";
    JsonObjectConst root = doc["result"].as<JsonObjectConst>();
    if (root.isNull()) root = doc.as<JsonObjectConst>();
    JsonArrayConst th = root["thumbnails"].as<JsonArrayConst>();
    if (th.isNull()) return "";
    int bestW = 0;
    std::string best;
    for (JsonObjectConst t : th) {
        int w = t["width"] | 0;
        const char* rp = t["relative_path"] | static_cast<const char*>(nullptr);
        if (rp && *rp && w > bestW) { bestW = w; best = rp; }
    }
    return best;
}

std::string buildThumbnailUrl(const std::string& base, const std::string& gcodeFilename,
                              const std::string& relativePath) {
    std::string dir;
    size_t slash = gcodeFilename.find_last_of('/');
    if (slash != std::string::npos) dir = gcodeFilename.substr(0, slash + 1);
    return base + "/server/files/gcodes/" + urlEncode(dir + relativePath);
}

// ---- Task 3: gcode builders ----

std::string buildJogScript(char axis, int deltaMm, int feed) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "G91\nG1 %c%d F%d\nG90", axis, deltaMm, feed);
    return buf;
}

std::string buildPreheatScript(int nozzle, int bed) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "M104 S%d\nM140 S%d", nozzle, bed);
    return buf;
}

// ---- JSON escape (RFC 8259) ----

std::string jsonEscape(const std::string& s) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) { out += "\\u00"; out += kHex[c >> 4]; out += kHex[c & 0x0F]; }
                else          { out += static_cast<char>(c); }
        }
    }
    return out;
}

// ---- Task 5: history ----

std::string cleanDisplayName(const std::string& filename) {
    std::string s = filename;
    size_t slash = s.find_last_of('/');
    if (slash != std::string::npos) s = s.substr(slash + 1);

    const std::string ext = ".gcode";
    if (s.size() > ext.size() &&
        s.compare(s.size() - ext.size(), ext.size(), ext) == 0)
        s = s.substr(0, s.size() - ext.size());

    for (char& c : s) if (c == '_') c = ' ';
    // Collapse runs of spaces (double '_' in slicer names) and trim edges.
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
        out += c;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    if (out.empty()) out = filename;
    return out;
}

std::vector<HistoryEntry> parseHistory(const std::string& json, size_t maxItems) {
    std::vector<HistoryEntry> out;
    JsonDocument doc;
    if (deserializeJson(doc, json)) return out;
    JsonObjectConst root = doc["result"].as<JsonObjectConst>();
    if (root.isNull()) root = doc.as<JsonObjectConst>();
    JsonArrayConst jobs = root["jobs"].as<JsonArrayConst>();
    if (jobs.isNull()) return out;

    std::vector<std::string> seen;              // most-recent job per filename wins
    for (JsonObjectConst job : jobs) {
        if (out.size() >= maxItems) break;
        const char* fn = job["filename"] | static_cast<const char*>(nullptr);
        if (!fn || !*fn) continue;
        std::string filename = fn;
        bool dup = false;
        for (const auto& s : seen) if (s == filename) { dup = true; break; }
        if (dup) continue;
        seen.push_back(filename);               // mark seen even if we skip it below
        bool exists = job["exists"] | false;
        if (!exists) continue;
        float et = job["end_time"]   | 0.0f;
        float st = job["start_time"] | 0.0f;
        float ts = (et > 0.f) ? et : st;
        if (ts <= 0.f) continue;               // no last-print time -> hide model
        out.push_back({filename, cleanDisplayName(filename),
                       static_cast<uint32_t>(ts + 0.5f)});
    }
    return out;
}

void formatPrintTime(uint32_t epoch, int tzOffsetHours, char* out, size_t n) {
    time_t t = static_cast<time_t>(epoch) + tzOffsetHours * 3600;
    struct tm tmv;
    gmtime_r(&t, &tmv);
    std::snprintf(out, n, "%02d.%02d %02d:%02d",
                  tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_hour, tmv.tm_min);
}

} // namespace paperos
