#pragma once
#include "services/HttpClient.h"
#include "services/PrinterStatus.h"
#include <string>
#include <vector>

namespace paperos {

class ConfigStore;
class Wifi;

// Facade over Moonraker's REST API. Owns an HttpClient (keep-alive) and
// brings WiFi up/down on demand, mirroring HAClient. Read-only except for
// the light toggle (gcode/script).
class MoonrakerClient {
public:
    MoonrakerClient(Wifi& w, ConfigStore& c) : wifi_(w), cfg_(c) {}

    void begin();                          // read config (no network)
    bool configured() const { return !url_.empty(); }

    bool ensureWifi();
    void releaseWifi();

    bool discoverLightObject();            // GET /printer/objects/list
    bool discoverPowerDevice();            // devices -> first, when config gave none
    bool pollStatus(PrinterStatus& out);   // POST /printer/objects/query
    bool setLight(bool on);                // POST /printer/gcode/script

    // --- control (Task 9) ---
    bool setPower(bool on);                                   // device_power on|off (bare key)
    PowerState fetchPower();                                  // device_power/device?device=
    bool jog(char axis, int deltaMm, int feed);              // gcode/script G91/G1/G90
    bool home();                                             // gcode/script G28
    bool firmwareRestart();                                  // printer/firmware_restart
    bool preheat(int nozzle, int bed);                       // gcode/script M104/M140
    bool cooldown();                                         // gcode/script TURN_OFF_HEATERS
    bool startPrint(const std::string& filename);            // print/start?filename=
    bool cancelPrint();                                      // print/cancel
    bool pausePrint();                                       // POST /printer/print/pause
    bool resumePrint();                                      // POST /printer/print/resume
    bool fetchHistory(std::vector<HistoryEntry>& out, size_t maxItems);

    // Everything we need from one /server/files/metadata GET (per file):
    struct FileMeta {
        uint32_t estimatedS = 0;                  // slicer estimated_time (0 = none)
        uint32_t gcodeStart = 0, gcodeEnd = 0;     // print-body byte range (0/0 = unknown)
        std::vector<uint8_t> thumbPng;             // largest thumbnail, empty = none
    };
    bool fetchFileMeta(const std::string& filename, FileMeta& out);

    const std::string& powerDevice() const { return powerDevice_; }
    int preheatNozzle() const { return preheatNozzle_; }
    int preheatBed() const { return preheatBed_; }

private:
    Wifi& wifi_;
    ConfigStore& cfg_;
    HttpClient http_;
    std::string url_, lightName_, lightOn_, lightOff_, lightObject_;
    std::string powerDevice_;
    int preheatNozzle_ = 200;
    int preheatBed_ = 60;
    static constexpr size_t kThumbMaxBytes = 32 * 1024;

    // One-entry cache: MoonrakerClient outlives PrinterApp (which is recreated
    // on every app entry), so re-entering during the same print skips the
    // network fetch for metadata/thumbnail already downloaded for this file.
    std::string meta_cache_file_;
    FileMeta meta_cache_;
};

} // namespace paperos
