#include <unity.h>
#include "apps/reader/EncodingDetect.h"
#include <vector>
#include <string>

using paperos::Encoding;
using paperos::detectEncoding;

void setUp() {}
void tearDown() {}

static std::vector<uint8_t> bytesOf(const char* s, size_t n) {
    return std::vector<uint8_t>(s, s + n);
}

void test_bom_utf8() {
    const uint8_t bom[] = {0xEF, 0xBB, 0xBF, 'h', 'i'};
    TEST_ASSERT_EQUAL(static_cast<int>(Encoding::Utf8),
                      static_cast<int>(detectEncoding(bom, sizeof(bom))));
}

void test_plain_ascii_is_utf8() {
    const uint8_t s[] = "Hello world.";
    TEST_ASSERT_EQUAL(static_cast<int>(Encoding::Utf8),
                      static_cast<int>(detectEncoding(s, sizeof(s) - 1)));
}

void test_russian_utf8() {
    // "Привет" in UTF-8
    const uint8_t s[] = {0xD0,0x9F,0xD1,0x80,0xD0,0xB8,0xD0,0xB2,0xD0,0xB5,0xD1,0x82};
    TEST_ASSERT_EQUAL(static_cast<int>(Encoding::Utf8),
                      static_cast<int>(detectEncoding(s, sizeof(s))));
}

void test_russian_cp1251() {
    // "Привет" in CP1251
    const uint8_t s[] = {0xCF,0xF0,0xE8,0xE2,0xE5,0xF2};
    TEST_ASSERT_EQUAL(static_cast<int>(Encoding::Cp1251),
                      static_cast<int>(detectEncoding(s, sizeof(s))));
}

void test_empty_buffer_defaults_to_utf8() {
    TEST_ASSERT_EQUAL(static_cast<int>(Encoding::Utf8),
                      static_cast<int>(detectEncoding(nullptr, 0)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_bom_utf8);
    RUN_TEST(test_plain_ascii_is_utf8);
    RUN_TEST(test_russian_utf8);
    RUN_TEST(test_russian_cp1251);
    RUN_TEST(test_empty_buffer_defaults_to_utf8);
    return UNITY_END();
}
