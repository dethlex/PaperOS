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

void test_printer_control_strings_exist_both_langs() {
    i18nSetLang(Lang::En);
    TEST_ASSERT_EQUAL_STRING("Power", tr(Str::printer_power));
    TEST_ASSERT_EQUAL_STRING("Yes", tr(Str::dlg_yes));
    TEST_ASSERT_NOT_NULL(strstr(tr(Str::printer_confirm_start_fmt), "%s"));
    i18nSetLang(Lang::Ru);
    TEST_ASSERT_EQUAL_STRING("Питание", tr(Str::printer_power));
    TEST_ASSERT_EQUAL_STRING("Да", tr(Str::dlg_yes));
    TEST_ASSERT_NOT_NULL(strstr(tr(Str::printer_confirm_start_fmt), "%s"));
}

void test_printer_redesign_strings() {
    i18nSetLang(Lang::En);
    TEST_ASSERT_EQUAL_STRING("Pause", tr(Str::printer_pause));
    TEST_ASSERT_EQUAL_STRING("Resume", tr(Str::printer_resume));
    TEST_ASSERT_NOT_NULL(strstr(tr(Str::printer_heat_fmt), "%d"));
    i18nSetLang(Lang::Ru);
    TEST_ASSERT_EQUAL_STRING("Пауза", tr(Str::printer_pause));
    TEST_ASSERT_EQUAL_STRING("Выключен", tr(Str::printer_powered_off));
    TEST_ASSERT_NOT_NULL(strstr(tr(Str::printer_heat_fmt), "%d"));
}

void test_calendar_strings() {
    i18nSetLang(Lang::En);
    TEST_ASSERT_EQUAL_STRING("Calendar", tr(Str::app_calendar));
    TEST_ASSERT_EQUAL_STRING("Today", tr(Str::cal_today));
    TEST_ASSERT_NOT_NULL(strstr(tr(Str::cal_more_fmt), "%d"));
    i18nSetLang(Lang::Ru);
    TEST_ASSERT_EQUAL_STRING("Календарь", tr(Str::app_calendar));
    TEST_ASSERT_EQUAL_STRING("Сегодня", tr(Str::cal_today));
    TEST_ASSERT_NOT_NULL(strstr(tr(Str::cal_test_ok_fmt), "%d"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_en);
    RUN_TEST(test_switch_to_en);
    RUN_TEST(test_ru_en_differ_but_format_preserved);
    RUN_TEST(test_out_of_range_falls_back);
    RUN_TEST(test_month_day_arrays);
    RUN_TEST(test_printer_control_strings_exist_both_langs);
    RUN_TEST(test_printer_redesign_strings);
    RUN_TEST(test_calendar_strings);
    return UNITY_END();
}
