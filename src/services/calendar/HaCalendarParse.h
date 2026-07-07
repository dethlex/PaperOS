#pragma once
#include "services/CalendarData.h"
#include <string>

namespace paperos {

// "2026-07-05T14:30:00+03:00" | "2026-07-05T11:30:00Z" → локальная эпоха
// (UTC + tz_offset_min). 0 при малформатном входе.
uint32_t parseIso8601ToLocalEpoch(const char* s, int16_t tz_offset_min);

// "2026-07-05" → локальная эпоха полуночи этого дня. 0 при ошибке.
uint32_t parseIsoDateToLocalEpoch(const char* s);

// Ответ HA /api/calendars/<entity> → CalendarData:
//  - dateTime → timed-событие, date → all-day (end.date эксклюзивный);
//  - события с end_local <= window_start_local отбрасываются;
//  - сортировка по start_local (all-day первыми при равенстве), обрезка до 32;
//  - title ← summary (48 Б, UTF-8-безопасная обрезка), location (32 Б);
//    отсутствующий summary → title остаётся пустым (рендер подставит i18n).
// false при битом JSON / не-массиве; out тогда не валиден.
bool parseHaCalendar(const std::string& json, uint32_t window_start_local,
                     int16_t tz_offset_min, CalendarData& out);

} // namespace paperos
