#include <unity.h>
#include "apps/screensaver/RotationPolicy.h"

using namespace paperos;
void setUp() {} void tearDown() {}

void test_cold_boot_rotates() {
    TEST_ASSERT_TRUE(rotationDue(1000, 0, 30));
}

void test_before_interval_no_rotate() {
    TEST_ASSERT_FALSE(rotationDue(1000 + 29 * 60, 1000, 30));
}

void test_at_interval_rotates() {
    TEST_ASSERT_TRUE(rotationDue(1000 + 30 * 60, 1000, 30));
}

void test_zero_setting_falls_back_to_30() {
    TEST_ASSERT_FALSE(rotationDue(1000 + 29 * 60, 1000, 0));
    TEST_ASSERT_TRUE(rotationDue(1000 + 30 * 60, 1000, 0));
}

void test_clock_moved_back_rotates() {
    // NTP resync jumped the clock backwards: rotating is harmless, getting
    // stuck for hours is not.
    TEST_ASSERT_TRUE(rotationDue(500, 1000, 30));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cold_boot_rotates);
    RUN_TEST(test_before_interval_no_rotate);
    RUN_TEST(test_at_interval_rotates);
    RUN_TEST(test_zero_setting_falls_back_to_30);
    RUN_TEST(test_clock_moved_back_rotates);
    return UNITY_END();
}
