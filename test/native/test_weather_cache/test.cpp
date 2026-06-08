#include <unity.h>
#include "services/WeatherCache.h"

using namespace paperos;
void setUp() {} void tearDown() {}

static WeatherData sample() {
    WeatherData w;
    w.valid = true;
    w.current_c = -3;
    w.current_wmo = 95;
    w.current_humidity = 64;
    w.fetched_unix = 123456789u;
    w.day_count = 3;
    w.days[0] = {20, 9, 3, 0};
    w.days[1] = {-1, -6, 71, 1};
    w.days[2] = {15, 7, 61, 2};
    w.hour_count = 3;
    w.hours[0] = {14, 18, 2};
    w.hours[1] = {15, -4, 3};
    w.hours[2] = {16, 17, 61};
    return w;
}

void test_round_trip() {
    WeatherData in = sample();
    uint8_t buf[160];
    size_t n = serializeWeather(in, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    WeatherData out;
    TEST_ASSERT_TRUE(deserializeWeather(buf, n, out));
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_EQUAL(in.current_c, out.current_c);
    TEST_ASSERT_EQUAL(in.current_wmo, out.current_wmo);
    TEST_ASSERT_EQUAL(in.current_humidity, out.current_humidity);
    TEST_ASSERT_EQUAL_UINT32(in.fetched_unix, out.fetched_unix);
    TEST_ASSERT_EQUAL(in.day_count, out.day_count);
    for (int i = 0; i < in.day_count; ++i) {
        TEST_ASSERT_EQUAL(in.days[i].tmax_c,   out.days[i].tmax_c);
        TEST_ASSERT_EQUAL(in.days[i].tmin_c,   out.days[i].tmin_c);
        TEST_ASSERT_EQUAL(in.days[i].wmo_code, out.days[i].wmo_code);
        TEST_ASSERT_EQUAL(in.days[i].weekday,  out.days[i].weekday);
    }
    TEST_ASSERT_EQUAL(in.hour_count, out.hour_count);
    for (int i = 0; i < in.hour_count; ++i) {
        TEST_ASSERT_EQUAL(in.hours[i].hour,     out.hours[i].hour);
        TEST_ASSERT_EQUAL(in.hours[i].temp_c,   out.hours[i].temp_c);
        TEST_ASSERT_EQUAL(in.hours[i].wmo_code, out.hours[i].wmo_code);
    }
}

void test_cap_too_small() {
    WeatherData in = sample();
    uint8_t buf[8];
    TEST_ASSERT_EQUAL_size_t(0, serializeWeather(in, buf, sizeof(buf)));
}

void test_empty_blob_invalid() {
    WeatherData out;
    TEST_ASSERT_FALSE(deserializeWeather(nullptr, 0, out));
    TEST_ASSERT_FALSE(out.valid);
}

void test_v1_blob_rejected() {
    uint8_t buf[16] = {1, 0, 0, 0, 0, 0, 0, 0, 0};
    WeatherData out;
    TEST_ASSERT_FALSE(deserializeWeather(buf, sizeof(buf), out));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_round_trip);
    RUN_TEST(test_cap_too_small);
    RUN_TEST(test_empty_blob_invalid);
    RUN_TEST(test_v1_blob_rejected);
    return UNITY_END();
}
