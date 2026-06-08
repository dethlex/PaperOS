#include "util/BatteryCurve.h"

namespace paperos {

uint8_t batteryPercentFromMv(uint16_t mv) {
    struct Pt { uint16_t mv; uint8_t pct; };
    // Таблица по убыванию напряжения. Между точками — линейная интерполяция.
    static const Pt kCurve[] = {
        {4200, 100}, {4100, 90}, {4000, 80}, {3900, 65}, {3800, 50},
        {3700, 35},  {3600, 20}, {3500, 10}, {3400, 5},  {3300, 0},
    };
    constexpr int N = sizeof(kCurve) / sizeof(kCurve[0]);

    if (mv >= kCurve[0].mv)       return 100;   // >= 4200
    if (mv <= kCurve[N - 1].mv)   return 0;     // <= 3300

    for (int i = 0; i < N - 1; ++i) {
        uint16_t hi_mv = kCurve[i].mv;          // больше
        uint16_t lo_mv = kCurve[i + 1].mv;      // меньше
        if (mv < hi_mv && mv >= lo_mv) {
            int span_mv = hi_mv - lo_mv;        // напр. 100
            int span_p  = kCurve[i].pct - kCurve[i + 1].pct;
            int d       = mv - lo_mv;
            int pct     = kCurve[i + 1].pct + (span_p * d + span_mv / 2) / span_mv;  // +rounding
            return static_cast<uint8_t>(pct);
        }
    }
    return 0;  // недостижимо: таблица непрерывна 3300..4200, любой mv в этом
               // диапазоне попадает ровно в один сегмент выше (клампы — снаружи).
}

} // namespace paperos
