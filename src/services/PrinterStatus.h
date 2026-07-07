#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace paperos {

// Parsed snapshot of Moonraker /printer/objects/query — what the printer
// app renders. Every field is optional at the wire level: a missing object
// or key leaves the field at its default here.
struct PrinterStatus {
    enum class Klippy  : uint8_t { Ready, Startup, Shutdown, Error, Unknown };
    enum class PrintSt : uint8_t { Standby, Printing, Paused, Complete, Cancelled, Error, Unknown };

    Klippy      klippy = Klippy::Unknown;
    std::string klippyMessage;            // webhooks.state_message
    PrintSt     state  = PrintSt::Unknown;
    std::string filename;                 // print_stats.filename
    float       progress       = 0.f;     // 0..1
    uint32_t    filePosition = 0;         // virtual_sdcard.file_position (bytes)
    uint32_t    printDurationS = 0;        // print_stats.print_duration (seconds)
    int         currentLayer   = -1;       // -1 = unknown
    int         totalLayer     = -1;
    float       hotendTemp = 0.f, hotendTarget = 0.f;
    float       bedTemp    = 0.f, bedTarget    = 0.f;
    bool        lightOn    = false;
    bool        lightKnown = false;        // false → LED state was not read
    std::string homedAxes;                 // toolhead.homed_axes, e.g. "xyz"; "" = unknown
};

// Estimated seconds remaining from file progress.
// Returns 0 when progress <= 0 or >= 1 (no meaningful estimate).
uint32_t etaRemainingSec(uint32_t printDurationS, float progress);

// Parse a Moonraker /printer/objects/query response body into `out`.
// `lightObject` is the exact object name (e.g. "led LED_Light") to read the
// LED state from; "" → light fields stay default (lightKnown=false).
// Returns false only on JSON parse error (out left untouched).
bool parsePrinterStatus(const std::string& json, const std::string& lightObject,
                        PrinterStatus& out);

// True if the two statuses render identically (progress to the percent,
// temps/times to the displayed granularity). Used to skip needless e-ink
// full refreshes during polling.
bool sameForDisplay(const PrinterStatus& a, const PrinterStatus& b);

// From a /printer/objects/list response body, pick the object whose name
// ends with " <lightName>" (e.g. "led LED_Light"). Prefers "led " over
// "neopixel " over any other prefix. Returns "" if none / parse error.
std::string findLightObject(const std::string& listJson, const std::string& lightName);

// First device name from /machine/device_power/devices ("" if none).
// Used to auto-discover the power device when printer.power_device is unset.
std::string parseFirstPowerDevice(const std::string& json);

// Percent-encode per RFC 3986, keeping unreserved chars and '/' (for gcode
// subpaths). Shared definition lives in util/WebDavPath.cpp.
std::string urlEncode(const std::string& s);

// Moonraker URL builders. NOTE the asymmetry: power on/off take the device name
// as a BARE query key (Moonraker's batch handler reads each query KEY as a device
// name), while the power STATUS GET reads the VALUE of a `device` param.
std::string buildPowerUrl(const std::string& base, const std::string& device, bool on);
std::string buildPowerStatusUrl(const std::string& base, const std::string& device);
std::string buildPrintStartUrl(const std::string& base, const std::string& filename);

// Slicer's total estimated print time (seconds) from /server/files/metadata.
// 0 on parse error / missing key. Accepts the {"result":…} envelope and bare top.
uint32_t parseSlicerEstimate(const std::string& json);
// URL for the file-metadata endpoint (filename url-encoded).
std::string buildMetadataUrl(const std::string& base, const std::string& filename);

// gcode_start_byte / gcode_end_byte from /server/files/metadata.
// False if either is missing or the range is degenerate (end <= start).
bool parseGcodeByteRange(const std::string& metadataJson, uint32_t& startB, uint32_t& endB);
// File-relative progress a la Fluidd: position inside [startB, endB], clamped 0..1.
float fileRelativeProgress(uint32_t pos, uint32_t startB, uint32_t endB);

// Largest thumbnail's relative_path from /server/files/metadata ("" if none).
std::string parseThumbnailPath(const std::string& metadataJson);
// Thumbnail download URL. relative_path is relative to the gcode file's dir.
std::string buildThumbnailUrl(const std::string& base, const std::string& gcodeFilename,
                              const std::string& relativePath);

// Relative-move gcode: switch to relative (G91), move one axis, restore absolute (G90).
std::string buildJogScript(char axis, int deltaMm, int feed);
// Non-blocking preheat: set nozzle + bed targets (no wait).
std::string buildPreheatScript(int nozzle, int bed);

// Escape a string for embedding inside a JSON string literal (RFC 8259):
// quotes, backslash, and control chars (\n \r \t and <0x20 as \u00XX).
std::string jsonEscape(const std::string& s);

enum class PowerState : uint8_t { Unknown, Off, On };

// Parse a Moonraker /machine/device_power/device response → On/Off for `device`.
// Unknown on parse error, missing device, or unrecognized value.
PowerState parsePowerState(const std::string& json, const std::string& device);

struct HistoryEntry {
    std::string filename;     // path for print/start, as Moonraker reports it
    std::string displayName;  // cleaned, for the UI list
    uint32_t    lastPrint = 0; // Unix epoch seconds (end_time, else start_time)
};

// Display name: strip dir + ".gcode", '_'->' ', collapse/trim spaces. Never empty.
std::string cleanDisplayName(const std::string& filename);

// Parse /server/history/list → most-recent-first, existing-only, de-duplicated by
// filename, capped at maxItems. Empty on parse error / no jobs.
// Entries with no usable timestamp (end_time or start_time) are skipped.
std::vector<HistoryEntry> parseHistory(const std::string& json, size_t maxItems);

// Format a Unix timestamp (UTC seconds) as "DD.MM HH:MM" in local time.
void formatPrintTime(uint32_t epoch, int tzOffsetHours, char* out, size_t n);

// Compact fixed-size cache persisted to NVS so the app can render the last
// known status instantly on enter (before WiFi connects). Filename is NOT
// cached (variable length); it re-appears on the first poll.
struct PrinterCache {
    uint8_t  version;        // = 1
    uint8_t  klippy;         // PrinterStatus::Klippy
    uint8_t  state;          // PrinterStatus::PrintSt
    uint8_t  flags;          // bit0 = lightOn, bit1 = lightKnown
    float    progress;
    uint32_t printDurationS;
    int16_t  currentLayer;   // -1 = unknown
    int16_t  totalLayer;
    int16_t  hotendTemp, hotendTarget, bedTemp, bedTarget;   // rounded °C
};
static_assert(sizeof(PrinterCache) == 24,
              "PrinterCache is persisted as a raw NVS blob; its size/layout must stay stable");

PrinterCache  toCache(const PrinterStatus& s);
PrinterStatus fromCache(const PrinterCache& c);   // version mismatch → default status

} // namespace paperos
