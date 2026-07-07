#include <unity.h>
#include <cstring>
#include "services/calendar/CalendarSelect.h"

using namespace paperos;
void setUp() {} void tearDown() {}

// Полночь «сегодня» — круглое число суток; полдень — now.
static const uint32_t D0  = 20000u * 86400u;
static const uint32_t NOW = D0 + 12u * 3600u;

static CalendarEvent ev(uint32_t s, uint32_t e, bool ad, const char* t) {
    CalendarEvent x;
    x.start_local = s; x.end_local = e; x.all_day = ad;
    std::strncpy(x.title, t, sizeof(x.title) - 1);
    return x;
}

void test_invalid_data_gives_nothing() {
    CalendarData d;   // valid=false
    TEST_ASSERT_NULL(pickScreensaverEvent(d, NOW).ev);
}

void test_ongoing_wins() {
    CalendarData d; d.valid = true; d.count = 2;
    d.events[0] = ev(NOW - 600, NOW + 600, false, "идёт");
    d.events[1] = ev(NOW + 3600, NOW + 7200, false, "потом");
    auto p = pickScreensaverEvent(d, NOW);
    TEST_ASSERT_NOT_NULL(p.ev);
    TEST_ASSERT_EQUAL_STRING("идёт", p.ev->title);
    TEST_ASSERT_TRUE(p.ongoing);
    TEST_ASSERT_FALSE(p.tomorrow);
}

void test_next_today() {
    CalendarData d; d.valid = true; d.count = 2;
    d.events[0] = ev(NOW - 7200, NOW - 3600, false, "прошло");
    d.events[1] = ev(NOW + 3600, NOW + 7200, false, "скоро");
    auto p = pickScreensaverEvent(d, NOW);
    TEST_ASSERT_NOT_NULL(p.ev);
    TEST_ASSERT_EQUAL_STRING("скоро", p.ev->title);
    TEST_ASSERT_FALSE(p.ongoing);
    TEST_ASSERT_FALSE(p.tomorrow);
}

void test_today_all_day_skipped() {
    CalendarData d; d.valid = true; d.count = 2;
    d.events[0] = ev(D0, D0 + 86400, true, "весь день");
    d.events[1] = ev(D0 + 86400 + 10u*3600, D0 + 86400 + 11u*3600, false, "завтра встреча");
    auto p = pickScreensaverEvent(d, NOW);
    TEST_ASSERT_NOT_NULL(p.ev);
    TEST_ASSERT_EQUAL_STRING("завтра встреча", p.ev->title);
    TEST_ASSERT_TRUE(p.tomorrow);
}

void test_tomorrow_all_day_shown() {
    CalendarData d; d.valid = true; d.count = 1;
    d.events[0] = ev(D0 + 86400, D0 + 2u*86400, true, "ДР");
    auto p = pickScreensaverEvent(d, NOW);
    TEST_ASSERT_NOT_NULL(p.ev);
    TEST_ASSERT_TRUE(p.tomorrow);
    TEST_ASSERT_EQUAL_STRING("ДР", p.ev->title);
}

void test_day_after_tomorrow_ignored() {
    CalendarData d; d.valid = true; d.count = 1;
    d.events[0] = ev(D0 + 2u*86400 + 3600, D0 + 2u*86400 + 7200, false, "послезавтра");
    TEST_ASSERT_NULL(pickScreensaverEvent(d, NOW).ev);
}

void test_boundary_event_start_equals_now_is_ongoing() {
    CalendarData d; d.valid = true; d.count = 1;
    d.events[0] = ev(NOW, NOW + 600, false, "ровно сейчас");
    auto p = pickScreensaverEvent(d, NOW);
    TEST_ASSERT_NOT_NULL(p.ev);
    TEST_ASSERT_TRUE(p.ongoing);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_invalid_data_gives_nothing);
    RUN_TEST(test_ongoing_wins);
    RUN_TEST(test_next_today);
    RUN_TEST(test_today_all_day_skipped);
    RUN_TEST(test_tomorrow_all_day_shown);
    RUN_TEST(test_day_after_tomorrow_ignored);
    RUN_TEST(test_boundary_event_start_equals_now_is_ongoing);
    return UNITY_END();
}
