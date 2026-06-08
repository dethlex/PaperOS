#include <unity.h>
#include "util/BatteryCurve.h"

using namespace paperos;
void setUp() {} void tearDown() {}

void test_exact_breakpoints() {
    TEST_ASSERT_EQUAL_UINT8(100, batteryPercentFromMv(4200));
    TEST_ASSERT_EQUAL_UINT8(90,  batteryPercentFromMv(4100));
    TEST_ASSERT_EQUAL_UINT8(80,  batteryPercentFromMv(4000));
    TEST_ASSERT_EQUAL_UINT8(65,  batteryPercentFromMv(3900));
    TEST_ASSERT_EQUAL_UINT8(50,  batteryPercentFromMv(3800));
    TEST_ASSERT_EQUAL_UINT8(35,  batteryPercentFromMv(3700));
    TEST_ASSERT_EQUAL_UINT8(20,  batteryPercentFromMv(3600));
    TEST_ASSERT_EQUAL_UINT8(10,  batteryPercentFromMv(3500));
    TEST_ASSERT_EQUAL_UINT8(5,   batteryPercentFromMv(3400));
    TEST_ASSERT_EQUAL_UINT8(0,   batteryPercentFromMv(3300));
}

void test_interpolation_midpoints() {
    // 3850 = 50mV into [3800..3900), span_p=15: 50 + round(15*50/100) = 58
    TEST_ASSERT_EQUAL_UINT8(58, batteryPercentFromMv(3850));  // between 50 and 65
    // 3950 = 50mV into [3900..4000), span_p=15: 65 + round(15*50/100) = 73
    TEST_ASSERT_EQUAL_UINT8(73, batteryPercentFromMv(3950));  // between 65 and 80
    // 3550 = 50mV into [3500..3600), span_p=10: 10 + round(10*50/100) = 15
    TEST_ASSERT_EQUAL_UINT8(15, batteryPercentFromMv(3550));  // between 10 and 20
}

void test_clamps() {
    TEST_ASSERT_EQUAL_UINT8(100, batteryPercentFromMv(4300));
    TEST_ASSERT_EQUAL_UINT8(100, batteryPercentFromMv(5000));
    // 4199 reaches 100 via the interpolation+rounding (not the early clamp).
    TEST_ASSERT_EQUAL_UINT8(100, batteryPercentFromMv(4199));
    TEST_ASSERT_EQUAL_UINT8(0,   batteryPercentFromMv(3200));
    TEST_ASSERT_EQUAL_UINT8(0,   batteryPercentFromMv(0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_exact_breakpoints);
    RUN_TEST(test_interpolation_midpoints);
    RUN_TEST(test_clamps);
    return UNITY_END();
}
