#pragma once
#include <M5EPD.h>
#include <time.h>

namespace paperos {

class Rtc {
public:
    void begin() {}
    // Read current time in unix seconds (RTC clock, no timezone applied).
    uint64_t nowUnix();
    // Set RTC absolute from a UTC unix epoch (gmtime fields written raw).
    void setUnix(uint64_t epoch);
    // Set RTC from an already-local `struct tm` (e.g. from getLocalTime, which
    // applied the UTC offset). Written as-is — the display reads the RTC raw, so
    // this is what makes the clock show local time. This is the NTP-sync path.
    void setLocalTime(const struct tm& lt);
    // NTP sync: assumes WiFi is ALREADY connected. Runs SNTP (pool.ntp.org) with
    // the given UTC offset (hours), waits up to 5s, and writes the resulting local
    // wall-clock into the RTC via setLocalTime. Returns true on success. Shared by
    // Settings → "Sync NTP" and the boot-time refresh in main.cpp.
    bool syncFromNtp(int tzOffsetHours);
    // Current minute of the hour (0..59) from the BM8563 — used by the clock-tick
    // path for hour/weather boundaries (replaces RTC-slow-memory minute_counter).
    int minute();
    // Convenience accessors.
    void formatHHMM(char* out, size_t n);   // "14:07" (current RTC time)
    void formatDate(char* out, size_t n);   // e.g. "Thu, 22 May" (localized)
    // "14:07" from an arbitrary unix timestamp (e.g. WeatherData::fetched_unix).
    // Uses gmtime — the inverse of nowUnix()'s mktime-as-UTC — so the wall-clock
    // stored at fetch time is recovered exactly.
    void formatHHMMUnix(uint64_t epoch, char* out, size_t n);
};

} // namespace paperos
