#include <unity.h>
#include "framework/LauncherOrder.h"

using namespace paperos;
void setUp() {} void tearDown() {}

void test_settings_pinned_last_order_preserved() {
    std::vector<std::string> in = {"reader","ha","settings","weather","fileserver"};
    auto out = launcherAppOrder(in);
    TEST_ASSERT_EQUAL_INT(5, (int)out.size());
    TEST_ASSERT_EQUAL_STRING("reader",     out[0].c_str());
    TEST_ASSERT_EQUAL_STRING("ha",         out[1].c_str());
    TEST_ASSERT_EQUAL_STRING("weather",    out[2].c_str());   // non-settings keep order
    TEST_ASSERT_EQUAL_STRING("fileserver", out[3].c_str());
    TEST_ASSERT_EQUAL_STRING("settings",   out[4].c_str());   // pinned last
}

void test_no_settings() {
    std::vector<std::string> in = {"reader","ha","weather"};
    auto out = launcherAppOrder(in);
    TEST_ASSERT_EQUAL_INT(3, (int)out.size());
    TEST_ASSERT_EQUAL_STRING("reader",  out[0].c_str());
    TEST_ASSERT_EQUAL_STRING("ha",      out[1].c_str());
    TEST_ASSERT_EQUAL_STRING("weather", out[2].c_str());
}

void test_empty_input() {
    auto out = launcherAppOrder({});
    TEST_ASSERT_EQUAL_INT(0, (int)out.size());
}

void test_only_settings_survives_filter() {
    auto out = launcherAppOrder({"launcher", "screensaver", "settings"});
    TEST_ASSERT_EQUAL_INT(1, (int)out.size());
    TEST_ASSERT_EQUAL_STRING("settings", out[0].c_str());
}

void test_new_app_after_settings_still_last() {
    std::vector<std::string> in = {"reader","settings","newapp"};
    auto out = launcherAppOrder(in);
    TEST_ASSERT_EQUAL_INT(3, (int)out.size());
    TEST_ASSERT_EQUAL_STRING("reader",   out[0].c_str());
    TEST_ASSERT_EQUAL_STRING("newapp",   out[1].c_str());
    TEST_ASSERT_EQUAL_STRING("settings", out[2].c_str());   // still last
}

void test_filters_service_ids() {
    std::vector<std::string> in = {"launcher","reader","screensaver","settings"};
    auto out = launcherAppOrder(in);
    TEST_ASSERT_EQUAL_INT(2, (int)out.size());
    TEST_ASSERT_EQUAL_STRING("reader",   out[0].c_str());
    TEST_ASSERT_EQUAL_STRING("settings", out[1].c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_settings_pinned_last_order_preserved);
    RUN_TEST(test_no_settings);
    RUN_TEST(test_new_app_after_settings_still_last);
    RUN_TEST(test_filters_service_ids);
    RUN_TEST(test_empty_input);
    RUN_TEST(test_only_settings_survives_filter);
    return UNITY_END();
}
