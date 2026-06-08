#include <unity.h>
#include "util/Logger.h"

void setUp() {}
void tearDown() {}

void test_log_macros_compile() {
    LOG_INFO("test", "hello %d", 42);
    LOG_WARN("test", "warn");
    LOG_ERROR("test", "err");
    TEST_PASS();
}

void test_level_filter_suppresses_debug() {
    paperos::log_set_min_level(paperos::LogLevel::Warn);
    LOG_DEBUG("test", "this should be suppressed");
    LOG_INFO("test", "this too");
    LOG_WARN("test", "this should print");
    paperos::log_set_min_level(paperos::LogLevel::Info);  // restore default
    TEST_PASS();
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_log_macros_compile);
    RUN_TEST(test_level_filter_suppresses_debug);
    return UNITY_END();
}
