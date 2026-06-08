#include "Logger.h"
#include <stdio.h>
#include <string.h>

#ifdef PAPEROS_NATIVE
  static inline void log_emit(const char* s) { fputs(s, stderr); }
#else
  #include <Arduino.h>
  static inline void log_emit(const char* s) { Serial.print(s); }
#endif

namespace paperos {

static LogLevel g_min = LogLevel::Info;

void log_set_min_level(LogLevel level) { g_min = level; }

static const char* level_str(LogLevel l) {
    switch (l) {
        case LogLevel::Debug: return "D";
        case LogLevel::Info:  return "I";
        case LogLevel::Warn:  return "W";
        case LogLevel::Error: return "E";
    }
    return "?";
}

void log_write(LogLevel level, const char* tag, const char* fmt, ...) {
    if (static_cast<int>(level) < static_cast<int>(g_min)) return;
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "[%s/%s] ", level_str(level), tag);
    if (n < 0) n = 0;
    if (n >= static_cast<int>(sizeof(buf))) n = static_cast<int>(sizeof(buf)) - 1;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    va_end(args);
    size_t len = strlen(buf);
    if (len >= sizeof(buf) - 1) len = sizeof(buf) - 2;
    buf[len] = '\n';
    buf[len + 1] = '\0';
    log_emit(buf);
}

} // namespace paperos
