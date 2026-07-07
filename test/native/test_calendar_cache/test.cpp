#include <unity.h>
#include <cstring>
#include "services/CalendarCache.h"

using namespace paperos;
void setUp() {} void tearDown() {}

static CalendarData sample() {
    CalendarData d;
    d.valid = true;
    d.fetched_unix = 1780000000u;
    d.count = 3;
    d.events[0].start_local = 1780000000u;
    d.events[0].end_local   = 1780001800u;
    d.events[0].all_day = false;
    std::strcpy(d.events[0].title, "Созвон с командой");
    std::strcpy(d.events[0].location, "Zoom");
    d.events[1].start_local = 1780038000u;
    d.events[1].end_local   = 1780124400u;
    d.events[1].all_day = true;
    std::strcpy(d.events[1].title, "День рождения Маши");
    d.events[2].start_local = 1780050000u;
    d.events[2].end_local   = 1780053600u;
    d.events[2].all_day = false;
    std::strcpy(d.events[2].title, "Спортзал");
    return d;
}

void test_round_trip() {
    CalendarData in = sample();
    uint8_t buf[3000];
    size_t n = serializeCalendar(in, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    CalendarData out;
    TEST_ASSERT_TRUE(deserializeCalendar(buf, n, out));
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_EQUAL_UINT32(in.fetched_unix, out.fetched_unix);
    TEST_ASSERT_EQUAL_UINT8(in.count, out.count);
    for (int i = 0; i < in.count; ++i) {
        TEST_ASSERT_EQUAL_UINT32(in.events[i].start_local, out.events[i].start_local);
        TEST_ASSERT_EQUAL_UINT32(in.events[i].end_local,   out.events[i].end_local);
        TEST_ASSERT_EQUAL(in.events[i].all_day, out.events[i].all_day);
        TEST_ASSERT_EQUAL_STRING(in.events[i].title,    out.events[i].title);
        TEST_ASSERT_EQUAL_STRING(in.events[i].location, out.events[i].location);
    }
}

void test_cap_too_small() {
    CalendarData in = sample();
    uint8_t buf[16];
    TEST_ASSERT_EQUAL_size_t(0, serializeCalendar(in, buf, sizeof(buf)));
}

void test_empty_blob_invalid() {
    CalendarData out;
    TEST_ASSERT_FALSE(deserializeCalendar(nullptr, 0, out));
    TEST_ASSERT_FALSE(out.valid);
}

void test_wrong_version_rejected() {
    uint8_t buf[8] = {9, 0, 0, 0, 0, 0};
    CalendarData out;
    TEST_ASSERT_FALSE(deserializeCalendar(buf, sizeof(buf), out));
}

void test_count_overflow_rejected() {
    // count больше kMaxCalendarEvents в заголовке → отказ, не переполнение.
    uint8_t buf[8] = {1, 200, 0, 0, 0, 0};
    CalendarData out;
    TEST_ASSERT_FALSE(deserializeCalendar(buf, sizeof(buf), out));
}

void test_truncated_records_rejected() {
    CalendarData in = sample();
    uint8_t buf[3000];
    size_t n = serializeCalendar(in, buf, sizeof(buf));
    CalendarData out;
    TEST_ASSERT_FALSE(deserializeCalendar(buf, n - 10, out));   // обрезанный хвост
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_round_trip);
    RUN_TEST(test_cap_too_small);
    RUN_TEST(test_empty_blob_invalid);
    RUN_TEST(test_wrong_version_rejected);
    RUN_TEST(test_count_overflow_rejected);
    RUN_TEST(test_truncated_records_rejected);
    return UNITY_END();
}
