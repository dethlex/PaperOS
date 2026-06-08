#include <unity.h>
#include <cstring>
#include "i18n/Strings.h"

using namespace paperos;
void setUp() {} void tearDown() {}

void test_default_is_en() {
    // Process default is English (overridden at boot from config). Runs first,
    // before any i18nSetLang() call, so it observes the initial g_lang.
    TEST_ASSERT_EQUAL_STRING("Games", tr(Str::app_games));
    TEST_ASSERT_EQUAL_STRING("Settings", tr(Str::app_settings));
}

void test_switch_to_en() {
    i18nSetLang(Lang::En);
    TEST_ASSERT_EQUAL_STRING("Games", tr(Str::app_games));
    TEST_ASSERT_EQUAL_STRING("Settings", tr(Str::app_settings));
    i18nSetLang(Lang::Ru);
}

void test_ru_en_differ_but_format_preserved() {
    i18nSetLang(Lang::Ru);
    const char* ru = tr(Str::fifteen_moves_fmt);
    i18nSetLang(Lang::En);
    const char* en = tr(Str::fifteen_moves_fmt);
    TEST_ASSERT_TRUE(strcmp(ru, en) != 0);
    TEST_ASSERT_NOT_NULL(strstr(ru, "%d"));
    TEST_ASSERT_NOT_NULL(strstr(en, "%d"));
    i18nSetLang(Lang::Ru);
}

void test_out_of_range_falls_back() {
    i18nSetLang(Lang::En);
    const char* s = tr(static_cast<Str>(static_cast<uint16_t>(Str::_Count) + 5));
    TEST_ASSERT_NOT_NULL(s);
    i18nSetLang(Lang::Ru);
}

void test_month_day_arrays() {
    i18nSetLang(Lang::Ru);
    TEST_ASSERT_EQUAL_STRING("янв", monthsShort()[0]);
    TEST_ASSERT_EQUAL_STRING("Вс",  daysShort()[0]);
    i18nSetLang(Lang::En);
    TEST_ASSERT_EQUAL_STRING("Jan", monthsShort()[0]);
    TEST_ASSERT_EQUAL_STRING("Sun", daysShort()[0]);
    i18nSetLang(Lang::Ru);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_en);
    RUN_TEST(test_switch_to_en);
    RUN_TEST(test_ru_en_differ_but_format_preserved);
    RUN_TEST(test_out_of_range_falls_back);
    RUN_TEST(test_month_day_arrays);
    return UNITY_END();
}
