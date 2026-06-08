#include "Rtc.h"
#include "i18n/Strings.h"
#include <stdio.h>

namespace paperos {

uint64_t Rtc::nowUnix() {
    rtc_date_t d; rtc_time_t t;
    M5.RTC.getDate(&d);
    M5.RTC.getTime(&t);
    struct tm tm{};
    tm.tm_year = d.year - 1900;
    tm.tm_mon  = d.mon - 1;
    tm.tm_mday = d.day;
    tm.tm_hour = t.hour;
    tm.tm_min  = t.min;
    tm.tm_sec  = t.sec;
    time_t e = mktime(&tm);
    return static_cast<uint64_t>(e);
}

void Rtc::setUnix(uint64_t epoch) {
    time_t e = static_cast<time_t>(epoch);
    struct tm* tm = gmtime(&e);
    rtc_date_t d = { static_cast<int8_t>(tm->tm_wday),
                     static_cast<int8_t>(tm->tm_mon + 1),
                     static_cast<int8_t>(tm->tm_mday),
                     static_cast<int16_t>(tm->tm_year + 1900) };
    rtc_time_t t = { static_cast<int8_t>(tm->tm_hour),
                     static_cast<int8_t>(tm->tm_min),
                     static_cast<int8_t>(tm->tm_sec) };
    M5.RTC.setDate(&d);
    M5.RTC.setTime(&t);
}

void Rtc::setLocalTime(const struct tm& lt) {
    rtc_date_t d = { static_cast<int8_t>(lt.tm_wday),
                     static_cast<int8_t>(lt.tm_mon + 1),
                     static_cast<int8_t>(lt.tm_mday),
                     static_cast<int16_t>(lt.tm_year + 1900) };
    rtc_time_t t = { static_cast<int8_t>(lt.tm_hour),
                     static_cast<int8_t>(lt.tm_min),
                     static_cast<int8_t>(lt.tm_sec) };
    M5.RTC.setDate(&d);
    M5.RTC.setTime(&t);
}

bool Rtc::syncFromNtp(int tzOffsetHours) {
    configTime(tzOffsetHours * 3600, 0, "pool.ntp.org");
    struct tm tm;
    if (!getLocalTime(&tm, 5000)) return false;
    // getLocalTime() already applied the UTC offset → `tm` is local wall-clock.
    // Store as-is (the display reads the RTC raw). NOT mktime()+setUnix(): that
    // round-trips back to UTC and loses the offset.
    setLocalTime(tm);
    return true;
}

int Rtc::minute() {
    rtc_time_t t; M5.RTC.getTime(&t);
    return t.min;
}

void Rtc::formatHHMM(char* out, size_t n) {
    rtc_time_t t; M5.RTC.getTime(&t);
    snprintf(out, n, "%02d:%02d", t.hour, t.min);
}

void Rtc::formatHHMMUnix(uint64_t epoch, char* out, size_t n) {
    time_t e = static_cast<time_t>(epoch);
    // localtime() is the exact inverse of nowUnix()'s mktime() under the same TZ.
    // After NTP sync configTime() sets the libc TZ (e.g. +3), so mktime() produced
    // a true UTC epoch; localtime() re-applies the offset to recover the local
    // wall-clock at fetch time. (gmtime() here would show UTC — 3 h off.)
    struct tm* tm = localtime(&e);
    if (!tm) { snprintf(out, n, "--:--"); return; }
    snprintf(out, n, "%02d:%02d", tm->tm_hour, tm->tm_min);
}

void Rtc::formatDate(char* out, size_t n) {
    rtc_date_t d; M5.RTC.getDate(&d);
    // rtc_date_t fields are int8_t; an uninitialised RTC can hold 0/junk and a
    // negative value would escape the array bounds — signed (0-1)%12 == -1 →
    // LoadProhibited inside snprintf -> strlen. Clamp before indexing.
    uint8_t wday = (d.week >= 0 && d.week < 7) ? static_cast<uint8_t>(d.week) : 0;
    uint8_t midx = (d.mon  >= 1 && d.mon  <= 12) ? static_cast<uint8_t>(d.mon - 1) : 0;
    snprintf(out, n, "%s, %d %s", daysShort()[wday], d.day, monthsShort()[midx]);
}

} // namespace paperos
