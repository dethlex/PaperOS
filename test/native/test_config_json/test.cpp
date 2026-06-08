#include <unity.h>
#include <cstring>
#include "services/ConfigJson.h"

using namespace paperos;
void setUp() {} void tearDown() {}

static bool has(const std::string& h, const char* n) { return h.find(n) != std::string::npos; }

void test_roundtrip() {
    Config a;
    a.wifiSsid="net"; a.wifiPass="pw"; a.haUrl="http://x"; a.haToken="tok";
    a.tzOffsetHours=5; a.weatherLat="55.7"; a.weatherLon="37.6";
    a.weatherRefreshMin=15; a.indoorTempOffset=-3;
    a.fontSize=2; a.marginPx=32; a.screensaverIdleS=600; a.photoRotateMin=60;
    Config b;  // defaults
    TEST_ASSERT_TRUE(parseConfigMerge(serializeConfig(a), b));
    TEST_ASSERT_EQUAL_STRING("net", b.wifiSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("pw",  b.wifiPass.c_str());
    TEST_ASSERT_EQUAL_STRING("http://x", b.haUrl.c_str());
    TEST_ASSERT_EQUAL_STRING("tok", b.haToken.c_str());
    TEST_ASSERT_EQUAL_INT8(5, b.tzOffsetHours);
    TEST_ASSERT_EQUAL_STRING("55.7", b.weatherLat.c_str());
    TEST_ASSERT_EQUAL_STRING("37.6", b.weatherLon.c_str());
    TEST_ASSERT_EQUAL_UINT16(15, b.weatherRefreshMin);
    TEST_ASSERT_EQUAL_INT8(-3, b.indoorTempOffset);
    TEST_ASSERT_EQUAL_UINT8(2, b.fontSize);
    TEST_ASSERT_EQUAL_UINT8(32, b.marginPx);
    TEST_ASSERT_EQUAL_UINT16(600, b.screensaverIdleS);
    TEST_ASSERT_EQUAL_UINT16(60, b.photoRotateMin);
}
void test_partial_merge_keeps_base() {
    Config base; base.wifiSsid="keep"; base.haToken="keeptok"; base.tzOffsetHours=2;
    const char* j = R"({"wifi":{"ssid":"newnet"},"weather":{"lat":"10.0"}})";
    TEST_ASSERT_TRUE(parseConfigMerge(j, base));
    TEST_ASSERT_EQUAL_STRING("newnet", base.wifiSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("keeptok", base.haToken.c_str());
    TEST_ASSERT_EQUAL_INT8(2, base.tzOffsetHours);
    TEST_ASSERT_EQUAL_STRING("10.0", base.weatherLat.c_str());
}
void test_empty_string_keeps_base() {
    Config base; base.wifiSsid="keep";
    TEST_ASSERT_TRUE(parseConfigMerge(R"({"wifi":{"ssid":""}})", base));
    TEST_ASSERT_EQUAL_STRING("keep", base.wifiSsid.c_str());
}
void test_number_invalid_kept() {
    Config base; base.weatherRefreshMin=30; base.tzOffsetHours=3;
    TEST_ASSERT_TRUE(parseConfigMerge(R"({"weather":{"refresh_min":999},"timezone_offset_hours":99})", base));
    TEST_ASSERT_EQUAL_UINT16(30, base.weatherRefreshMin);
    TEST_ASSERT_EQUAL_INT8(3, base.tzOffsetHours);
}
void test_number_valid_applied() {
    Config base; base.weatherRefreshMin=30;
    TEST_ASSERT_TRUE(parseConfigMerge(R"({"weather":{"refresh_min":15}})", base));
    TEST_ASSERT_EQUAL_UINT16(15, base.weatherRefreshMin);
}
void test_idle_s_out_of_uint16_kept() {
    // 86400 > uint16_t max (65535): не должно молча обрезаться — поле сохраняет base.
    Config base; base.screensaverIdleS=300;
    TEST_ASSERT_TRUE(parseConfigMerge(R"({"screensaver":{"idle_s":86400}})", base));
    TEST_ASSERT_EQUAL_UINT16(300, base.screensaverIdleS);
    // граничное валидное значение применяется.
    TEST_ASSERT_TRUE(parseConfigMerge(R"({"screensaver":{"idle_s":65535}})", base));
    TEST_ASSERT_EQUAL_UINT16(65535, base.screensaverIdleS);
}
void test_bad_json_false_unchanged() {
    Config base; base.wifiSsid="keep";
    TEST_ASSERT_FALSE(parseConfigMerge("{not json", base));
    TEST_ASSERT_EQUAL_STRING("keep", base.wifiSsid.c_str());
}
void test_serialize_has_all_sections() {
    std::string j = serializeConfig(Config{});
    TEST_ASSERT_TRUE(has(j, "\"wifi\""));
    TEST_ASSERT_TRUE(has(j, "\"ha\""));
    TEST_ASSERT_TRUE(has(j, "\"weather\""));
    TEST_ASSERT_TRUE(has(j, "\"reader\""));
    TEST_ASSERT_TRUE(has(j, "\"screensaver\""));
    TEST_ASSERT_TRUE(has(j, "timezone_offset_hours"));
}
void test_language_roundtrip() {
    Config c; c.language = 1;                       // en
    std::string j = serializeConfig(c);
    TEST_ASSERT_NOT_NULL(strstr(j.c_str(), "\"language\""));
    Config back;                                    // default language = 1 (en)
    TEST_ASSERT_EQUAL_UINT8(1, back.language);      // default is now English
    TEST_ASSERT_TRUE(parseConfigMerge(j, back));
    TEST_ASSERT_EQUAL_UINT8(1, back.language);
    Config d; d.language = 0;                        // unknown value leaves the prior value as-is
    TEST_ASSERT_TRUE(parseConfigMerge("{\"language\":\"xx\"}", d));
    TEST_ASSERT_EQUAL_UINT8(0, d.language);

    Config r; r.language = 0;                       // ru
    std::string jr = serializeConfig(r);
    TEST_ASSERT_NOT_NULL(strstr(jr.c_str(), "\"ru\""));
    Config rback; rback.language = 1;               // start from En to prove overwrite
    TEST_ASSERT_TRUE(parseConfigMerge(jr, rback));
    TEST_ASSERT_EQUAL_UINT8(0, rback.language);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_partial_merge_keeps_base);
    RUN_TEST(test_empty_string_keeps_base);
    RUN_TEST(test_number_invalid_kept);
    RUN_TEST(test_number_valid_applied);
    RUN_TEST(test_idle_s_out_of_uint16_kept);
    RUN_TEST(test_bad_json_false_unchanged);
    RUN_TEST(test_serialize_has_all_sections);
    RUN_TEST(test_language_roundtrip);
    return UNITY_END();
}
