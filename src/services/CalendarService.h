#pragma once
#include "services/CalendarData.h"
#include "services/HttpClient.h"
#include <string>
#include <vector>

namespace paperos {

class Wifi;
class ConfigStore;
class Rtc;

// Networking facade, mirrors WeatherService. Владеет своим HttpClient (паттерн
// «сервис владеет клиентом», как Weather/Moonraker) и ходит в HA REST API под
// существующим long-lived токеном. Rtc нужен для вычисления окна запроса
// [сегодня 00:00; +7 суток) и штампа fetched_unix. loadCache/saveCache без сети.
class CalendarService {
public:
    static constexpr uint8_t kWindowDays = 7;

    CalendarService(Wifi& w, ConfigStore& c, Rtc& r) : wifi_(w), cfg_(c), rtc_(r) {}

    void begin();                     // читает haUrl/haToken/calendarEntity
    bool configured() const;          // после begin(): url+token+entity непустые
    bool ensureWifi();
    void releaseWifi();
    bool fetch(CalendarData& out);    // GET событий окна; сам ставит fetched_unix
    bool fetchCalendarList(std::vector<CalendarInfo>& out);   // GET /api/calendars
    bool loadCache(CalendarData& out);
    void saveCache(const CalendarData& d);

private:
    // Локальная эпоха → "YYYY-MM-DDTHH:MM:SSZ" (UTC). '+' в query-строке
    // декодируется как пробел, поэтому офсеты не используем — только Z.
    std::string isoUtcFromLocal(uint32_t local_epoch) const;

    Wifi& wifi_;
    ConfigStore& cfg_;
    Rtc& rtc_;
    HttpClient http_;
    std::string url_, token_, entity_;
};

} // namespace paperos
