#pragma once
#include <stdarg.h>

namespace paperos {

enum class LogLevel { Debug, Info, Warn, Error };

void log_set_min_level(LogLevel level);
void log_write(LogLevel level, const char* tag, const char* fmt, ...);

} // namespace paperos

#define LOG_DEBUG(tag, ...) ::paperos::log_write(::paperos::LogLevel::Debug, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...)  ::paperos::log_write(::paperos::LogLevel::Info,  tag, __VA_ARGS__)
#define LOG_WARN(tag, ...)  ::paperos::log_write(::paperos::LogLevel::Warn,  tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) ::paperos::log_write(::paperos::LogLevel::Error, tag, __VA_ARGS__)
