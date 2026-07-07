#pragma once
#include "services/CalendarData.h"

namespace paperos {

// Что показать на скринсейвере рядом с часами. Правила — спека §8.
struct NextEventPick {
    const CalendarEvent* ev = nullptr;   // nullptr = строка не рисуется
    bool tomorrow = false;               // true → префикс «Завтра»
    bool ongoing  = false;               // true → "HH:MM–HH:MM Title"
};

// `now` — локальная эпоха (rtc.nowUnix()). d может быть invalid — вернём пусто.
NextEventPick pickScreensaverEvent(const CalendarData& d, uint32_t now);

} // namespace paperos
