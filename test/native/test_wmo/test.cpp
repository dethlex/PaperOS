#include <unity.h>
#include "util/WmoCode.h"
#include "i18n/Strings.h"

using namespace paperos;
void setUp() {} void tearDown() {}

void test_cat_mapping() {
    TEST_ASSERT_EQUAL(int(WeatherCat::Clear),        int(catFromWmo(0)));
    TEST_ASSERT_EQUAL(int(WeatherCat::PartlyCloudy), int(catFromWmo(2)));
    TEST_ASSERT_EQUAL(int(WeatherCat::Cloudy),       int(catFromWmo(3)));
    TEST_ASSERT_EQUAL(int(WeatherCat::Fog),          int(catFromWmo(45)));
    TEST_ASSERT_EQUAL(int(WeatherCat::Rain),         int(catFromWmo(63)));
    TEST_ASSERT_EQUAL(int(WeatherCat::Rain),         int(catFromWmo(81)));
    TEST_ASSERT_EQUAL(int(WeatherCat::Snow),         int(catFromWmo(73)));
    TEST_ASSERT_EQUAL(int(WeatherCat::Thunder),      int(catFromWmo(95)));
    TEST_ASSERT_EQUAL(int(WeatherCat::Unknown),      int(catFromWmo(200)));
}

void test_label_ids() {
    TEST_ASSERT_EQUAL(int(Str::wmo_clear),   int(wmoLabelId(WeatherCat::Clear)));
    TEST_ASSERT_EQUAL(int(Str::wmo_rain),    int(wmoLabelId(WeatherCat::Rain)));
    TEST_ASSERT_EQUAL(int(Str::wmo_unknown), int(wmoLabelId(WeatherCat::Unknown)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cat_mapping);
    RUN_TEST(test_label_ids);
    return UNITY_END();
}
