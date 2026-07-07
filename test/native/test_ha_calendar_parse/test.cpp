#include <unity.h>
#include <cstring>
#include "services/calendar/HaCalendarParse.h"

using namespace paperos;
void setUp() {} void tearDown() {}

// 2026-07-05 00:00 UTC = 20639 суток от эпохи × 86400 = 1783209600.
// Пересчёт руками: 1970..2025 = 56 лет × 365 + 14 високосных = 20454 дня до
// 2026-01-01; + 181 (янв..июн) + 4 = 20639.
static const uint32_t kDayUtc = 1783209600u;

void test_iso_with_positive_offset() {
    // 14:30:00+03:00 = 11:30 UTC; tz устройства +180 мин → локально 14:30.
    uint32_t v = parseIso8601ToLocalEpoch("2026-07-05T14:30:00+03:00", 180);
    TEST_ASSERT_EQUAL_UINT32(kDayUtc + 14u*3600 + 30u*60, v);
}

void test_iso_zulu() {
    uint32_t v = parseIso8601ToLocalEpoch("2026-07-05T11:30:00Z", 180);
    TEST_ASSERT_EQUAL_UINT32(kDayUtc + 14u*3600 + 30u*60, v);
}

void test_iso_negative_offset() {
    // 06:30:00-05:00 = 11:30 UTC → локально (tz +180) 14:30.
    uint32_t v = parseIso8601ToLocalEpoch("2026-07-05T06:30:00-05:00", 180);
    TEST_ASSERT_EQUAL_UINT32(kDayUtc + 14u*3600 + 30u*60, v);
}

void test_iso_half_hour_offset() {
    // +05:30 (Индия): 17:00+05:30 = 11:30 UTC → локально 14:30.
    uint32_t v = parseIso8601ToLocalEpoch("2026-07-05T17:00:00+05:30", 180);
    TEST_ASSERT_EQUAL_UINT32(kDayUtc + 14u*3600 + 30u*60, v);
}

void test_iso_malformed() {
    TEST_ASSERT_EQUAL_UINT32(0, parseIso8601ToLocalEpoch("garbage", 180));
    TEST_ASSERT_EQUAL_UINT32(0, parseIso8601ToLocalEpoch("", 180));
    TEST_ASSERT_EQUAL_UINT32(0, parseIso8601ToLocalEpoch("2026-07-05", 180));  // нет времени
}

void test_iso_date_only() {
    // Дата без времени = локальная полночь (без TZ-сдвига).
    uint32_t v = parseIsoDateToLocalEpoch("2026-07-05");
    TEST_ASSERT_EQUAL_UINT32(kDayUtc, v);
    TEST_ASSERT_EQUAL_UINT32(0, parseIsoDateToLocalEpoch("2026-7"));
}

// --- parseHaCalendar ---

static const char* kJsonTwoEvents = R"([
  {"summary":"Спортзал","start":{"dateTime":"2026-07-05T19:00:00+03:00"},
   "end":{"dateTime":"2026-07-05T20:30:00+03:00"}},
  {"summary":"Созвон с командой","start":{"dateTime":"2026-07-05T14:30:00+03:00"},
   "end":{"dateTime":"2026-07-05T15:00:00+03:00"},"location":"Zoom"}
])";

void test_parse_sorts_by_start() {
    CalendarData d;
    TEST_ASSERT_TRUE(parseHaCalendar(kJsonTwoEvents, 0, 180, d));
    TEST_ASSERT_TRUE(d.valid);
    TEST_ASSERT_EQUAL_UINT8(2, d.count);
    TEST_ASSERT_EQUAL_STRING("Созвон с командой", d.events[0].title);
    TEST_ASSERT_EQUAL_STRING("Zoom", d.events[0].location);
    TEST_ASSERT_EQUAL_STRING("Спортзал", d.events[1].title);
    TEST_ASSERT_TRUE(d.events[0].start_local < d.events[1].start_local);
}

void test_parse_all_day_exclusive_end() {
    const char* j = R"([{"summary":"ДР","start":{"date":"2026-07-06"},"end":{"date":"2026-07-07"}}])";
    CalendarData d;
    TEST_ASSERT_TRUE(parseHaCalendar(j, 0, 180, d));
    TEST_ASSERT_EQUAL_UINT8(1, d.count);
    TEST_ASSERT_TRUE(d.events[0].all_day);
    TEST_ASSERT_EQUAL_UINT32(kDayUtc + 86400u,     d.events[0].start_local);
    TEST_ASSERT_EQUAL_UINT32(kDayUtc + 2u*86400u,  d.events[0].end_local);
}

void test_parse_drops_ended_before_window() {
    CalendarData d;
    // Окно начинается после конца обоих событий → пусто, но valid.
    uint32_t late = kDayUtc + 23u*3600;
    TEST_ASSERT_TRUE(parseHaCalendar(kJsonTwoEvents, late, 180, d));
    TEST_ASSERT_TRUE(d.valid);
    TEST_ASSERT_EQUAL_UINT8(0, d.count);
}

void test_parse_truncates_to_max() {
    std::string j = "[";
    for (int i = 0; i < 40; ++i) {
        char ev[256];
        snprintf(ev, sizeof(ev),
            "%s{\"summary\":\"E%d\",\"start\":{\"dateTime\":\"2026-07-05T%02d:%02d:00Z\"},"
            "\"end\":{\"dateTime\":\"2026-07-05T%02d:%02d:00Z\"}}",
            i ? "," : "", i, i / 4, (i % 4) * 15, i / 4, (i % 4) * 15 + 10);
        j += ev;
    }
    j += "]";
    CalendarData d;
    TEST_ASSERT_TRUE(parseHaCalendar(j, 0, 0, d));
    TEST_ASSERT_EQUAL_UINT8(kMaxCalendarEvents, d.count);
    TEST_ASSERT_EQUAL_STRING("E0", d.events[0].title);   // ранние выжили
}

void test_parse_utf8_truncation_no_broken_char() {
    // 30 кириллических букв = 60 байт > 47 доступных → обрезка на границе символа.
    const char* j = R"([{"summary":"ФёдорДостоевскийПреступлениеИНаказание",
      "start":{"dateTime":"2026-07-05T10:00:00Z"},
      "end":{"dateTime":"2026-07-05T11:00:00Z"}}])";
    CalendarData d;
    TEST_ASSERT_TRUE(parseHaCalendar(j, 0, 0, d));
    size_t len = strlen(d.events[0].title);
    TEST_ASSERT_TRUE(len > 0 && len < 48);
    TEST_ASSERT_EQUAL(0, len % 2);   // кириллица UTF-8 = 2 байта/символ; чётная длина = не разорвали
}

void test_parse_missing_summary_and_end() {
    const char* j = R"([{"start":{"dateTime":"2026-07-05T10:00:00Z"}}])";
    CalendarData d;
    TEST_ASSERT_TRUE(parseHaCalendar(j, 0, 0, d));
    TEST_ASSERT_EQUAL_UINT8(1, d.count);
    TEST_ASSERT_EQUAL_STRING("", d.events[0].title);
    // end отсутствует → end = start (событие нулевой длины, не мусор).
    TEST_ASSERT_EQUAL_UINT32(d.events[0].start_local, d.events[0].end_local);
}

void test_parse_bad_json() {
    CalendarData d;
    TEST_ASSERT_FALSE(parseHaCalendar("{not json", 0, 0, d));
    TEST_ASSERT_FALSE(d.valid);
    TEST_ASSERT_FALSE(parseHaCalendar(R"({"a":1})", 0, 0, d));   // не массив
}

void test_parse_empty_array_ok() {
    CalendarData d;
    TEST_ASSERT_TRUE(parseHaCalendar("[]", 0, 0, d));
    TEST_ASSERT_TRUE(d.valid);
    TEST_ASSERT_EQUAL_UINT8(0, d.count);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_iso_with_positive_offset);
    RUN_TEST(test_iso_zulu);
    RUN_TEST(test_iso_negative_offset);
    RUN_TEST(test_iso_half_hour_offset);
    RUN_TEST(test_iso_malformed);
    RUN_TEST(test_iso_date_only);
    RUN_TEST(test_parse_sorts_by_start);
    RUN_TEST(test_parse_all_day_exclusive_end);
    RUN_TEST(test_parse_drops_ended_before_window);
    RUN_TEST(test_parse_truncates_to_max);
    RUN_TEST(test_parse_utf8_truncation_no_broken_char);
    RUN_TEST(test_parse_missing_summary_and_end);
    RUN_TEST(test_parse_bad_json);
    RUN_TEST(test_parse_empty_array_ok);
    return UNITY_END();
}
