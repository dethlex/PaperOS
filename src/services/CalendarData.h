#pragma once
#include <stdint.h>
#include <string>

namespace paperos {

constexpr uint8_t kMaxCalendarEvents = 32;

// Времена — «локальная эпоха» (см. спеку §5): unix-секунды, в которых RTC
// устройства живёт локальным временем. now % 86400 = секунды от полуночи.
struct CalendarEvent {
    uint32_t start_local = 0;
    uint32_t end_local   = 0;    // для all-day — 00:00 дня ПОСЛЕ последнего (эксклюзивно)
    bool     all_day     = false;
    char     title[48]   = {};   // UTF-8, обрезка без разрыва символа
    char     location[32] = {};  // UTF-8, может быть пустым
};

struct CalendarData {
    bool          valid = false;
    uint8_t       count = 0;                        // 0..kMaxCalendarEvents
    CalendarEvent events[kMaxCalendarEvents];       // отсортированы по start_local
    uint32_t      fetched_unix = 0;                 // локальная эпоха момента фетча
};

// Для пикера календарей в Settings (ответ GET /api/calendars).
struct CalendarInfo {
    std::string entity_id;   // "calendar.lexis"
    std::string name;        // "lexis@..."
};

} // namespace paperos
