#include "CalendarSelect.h"

namespace paperos {

NextEventPick pickScreensaverEvent(const CalendarData& d, uint32_t now) {
    NextEventPick pick;
    if (!d.valid || d.count == 0) return pick;

    const uint32_t today0    = now - now % 86400u;
    const uint32_t tomorrow0 = today0 + 86400u;
    const uint32_t tomorrow1 = tomorrow0 + 86400u;

    // 1) Идущее сейчас timed-событие (события отсортированы по start).
    for (uint8_t i = 0; i < d.count; ++i) {
        const CalendarEvent& e = d.events[i];
        if (!e.all_day && e.start_local <= now && now < e.end_local) {
            pick.ev = &e; pick.ongoing = true;
            return pick;
        }
    }
    // 2) Ближайшее будущее timed сегодня.
    for (uint8_t i = 0; i < d.count; ++i) {
        const CalendarEvent& e = d.events[i];
        if (!e.all_day && e.start_local > now && e.start_local < tomorrow0) {
            pick.ev = &e;
            return pick;
        }
    }
    // 3) Первое завтра (timed или all-day, стартующее завтра).
    for (uint8_t i = 0; i < d.count; ++i) {
        const CalendarEvent& e = d.events[i];
        if (e.start_local >= tomorrow0 && e.start_local < tomorrow1) {
            pick.ev = &e; pick.tomorrow = true;
            return pick;
        }
    }
    return pick;   // 4) пусто
}

} // namespace paperos
