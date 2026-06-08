#include <unity.h>
#include "services/weather/OpenMeteoParse.h"

using namespace paperos;
void setUp() {} void tearDown() {}

static const char* kSample = R"({
  "current": {"temperature_2m": 17.4, "relative_humidity_2m": 63, "weather_code": 2},
  "hourly": {
    "time": ["2026-06-01T14:00","2026-06-01T15:00","2026-06-01T16:00"],
    "temperature_2m": [18.2, -4.4, 16.1],
    "weather_code": [2, 3, 61]
  },
  "daily": {
    "time": ["2026-06-01","2026-06-02","2026-06-03","2026-06-04","2026-06-05"],
    "weather_code": [3, 61, 0, 95, 71],
    "temperature_2m_max": [19.6, 15.1, 22.0, 18.3, -1.2],
    "temperature_2m_min": [9.1, 8.0, 11.5, 10.0, -5.8]
  }
})";

void test_parse_current_and_days() {
    WeatherData w;
    TEST_ASSERT_TRUE(parseOpenMeteo(kSample, w));
    TEST_ASSERT_TRUE(w.valid);
    TEST_ASSERT_EQUAL(17, w.current_c);          // 17.4 -> 17
    TEST_ASSERT_EQUAL(2,  w.current_wmo);
    TEST_ASSERT_EQUAL(5,  w.day_count);
    TEST_ASSERT_EQUAL(3,  w.days[0].wmo_code);
    TEST_ASSERT_EQUAL(20, w.days[0].tmax_c);     // 19.6 -> 20
    TEST_ASSERT_EQUAL(9,  w.days[0].tmin_c);     // 9.1  -> 9
    TEST_ASSERT_EQUAL(-1, w.days[4].tmax_c);     // -1.2 -> -1
    TEST_ASSERT_EQUAL(-6, w.days[4].tmin_c);     // -5.8 -> -6
}

void test_current_humidity() {
    WeatherData w;
    TEST_ASSERT_TRUE(parseOpenMeteo(kSample, w));
    TEST_ASSERT_EQUAL(63, w.current_humidity);
}

void test_hourly_parsed() {
    WeatherData w;
    TEST_ASSERT_TRUE(parseOpenMeteo(kSample, w));
    TEST_ASSERT_EQUAL(3,  w.hour_count);
    TEST_ASSERT_EQUAL(14, w.hours[0].hour);
    TEST_ASSERT_EQUAL(18, w.hours[0].temp_c);     // 18.2 -> 18
    TEST_ASSERT_EQUAL(2,  w.hours[0].wmo_code);
    TEST_ASSERT_EQUAL(15, w.hours[1].hour);
    TEST_ASSERT_EQUAL(-4, w.hours[1].temp_c);     // -4.4 -> -4
    TEST_ASSERT_EQUAL(16, w.hours[2].hour);
    TEST_ASSERT_EQUAL(61, w.hours[2].wmo_code);
}

void test_weekday_derived_from_date() {
    WeatherData w;
    TEST_ASSERT_TRUE(parseOpenMeteo(kSample, w));
    TEST_ASSERT_EQUAL(0, w.days[0].weekday);     // 2026-06-01 is Monday
    TEST_ASSERT_EQUAL(1, w.days[1].weekday);     // +1 day
    TEST_ASSERT_EQUAL(4, w.days[4].weekday);     // Friday
}

void test_invalid_json() {
    WeatherData w;
    TEST_ASSERT_FALSE(parseOpenMeteo("not json", w));
    TEST_ASSERT_FALSE(w.valid);
}

void test_missing_current() {
    WeatherData w;
    TEST_ASSERT_FALSE(parseOpenMeteo(R"({"daily":{}})", w));
}

void test_missing_daily() {
    WeatherData w;
    TEST_ASSERT_TRUE(parseOpenMeteo(R"({"current":{"temperature_2m":5.0,"weather_code":0}})", w));
    TEST_ASSERT_TRUE(w.valid);
    TEST_ASSERT_EQUAL(0, w.day_count);
    TEST_ASSERT_EQUAL(5, w.current_c);
}

void test_day_count_clamped_to_max() {
    WeatherData w;
    const char* j = R"({
      "current": {"temperature_2m": 1.0, "weather_code": 0},
      "daily": {
        "time": ["2026-06-01","2026-06-02","2026-06-03","2026-06-04","2026-06-05","2026-06-06","2026-06-07"],
        "weather_code": [0,0,0,0,0,0,0],
        "temperature_2m_max": [1,2,3,4,5,6,7],
        "temperature_2m_min": [0,0,0,0,0,0,0]
      }
    })";
    TEST_ASSERT_TRUE(parseOpenMeteo(j, w));
    TEST_ASSERT_EQUAL(5, w.day_count);   // clamped to kMaxDays
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_current_and_days);
    RUN_TEST(test_current_humidity);
    RUN_TEST(test_hourly_parsed);
    RUN_TEST(test_weekday_derived_from_date);
    RUN_TEST(test_invalid_json);
    RUN_TEST(test_missing_current);
    RUN_TEST(test_missing_daily);
    RUN_TEST(test_day_count_clamped_to_max);
    return UNITY_END();
}
