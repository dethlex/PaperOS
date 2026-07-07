#include <unity.h>
#include <string.h>
#include "util/Utf8.h"

using namespace paperos;
void setUp() {} void tearDown() {}

void test_count_ascii() {
    TEST_ASSERT_EQUAL(5, utf8CountCp("hello", 5));
    TEST_ASSERT_EQUAL(0, utf8CountCp("", 0));
}

void test_count_cyrillic() {
    // "Привет" — 6 codepoints, 12 bytes.
    const char* s = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    TEST_ASSERT_EQUAL(6, utf8CountCp(s, strlen(s)));
}

void test_count_mixed() {
    // "a€b" — 'a' (1B) + U+20AC (3B) + 'b' (1B) = 3 codepoints, 5 bytes.
    const char* s = "a\xE2\x82\xAC" "b";
    TEST_ASSERT_EQUAL(3, utf8CountCp(s, strlen(s)));
}

void test_advance() {
    const char* s = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"; // Привет
    size_t len = strlen(s);
    TEST_ASSERT_EQUAL(4,  utf8AdvanceCp(s, len, 0, 2));   // 2 cp = 4 bytes
    TEST_ASSERT_EQUAL(len, utf8AdvanceCp(s, len, 0, 99)); // clamp to end
    TEST_ASSERT_EQUAL(6,  utf8AdvanceCp(s, len, 4, 1));   // from mid-string
}

void test_clip_no_truncation() {
    char dst[32];
    size_t n = utf8Clip(dst, sizeof(dst), "hello", 10);
    TEST_ASSERT_EQUAL_STRING("hello", dst);
    TEST_ASSERT_EQUAL(5, n);
}

void test_clip_truncates_with_ellipsis() {
    char dst[32];
    // "Привет" clipped to 3 cp → "При" (6 bytes) + "…" (3 bytes).
    utf8Clip(dst, sizeof(dst), "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", 3);
    TEST_ASSERT_EQUAL_STRING("\xD0\x9F\xD1\x80\xD0\xB8\xE2\x80\xA6", dst);
}

void test_clip_exact_fit_no_ellipsis() {
    char dst[32];
    utf8Clip(dst, sizeof(dst), "abc", 3);
    TEST_ASSERT_EQUAL_STRING("abc", dst);       // consumed fully → no ellipsis
}

void test_clip_cap_bound() {
    // cap too small for the whole string: must stop early, still NUL-terminate,
    // never overflow, and signal truncation with the ellipsis.
    char dst[8];                                 // 8 bytes: ≤3 payload + 3 "…" + NUL
    utf8Clip(dst, sizeof(dst), "abcdefgh", 99);
    TEST_ASSERT_EQUAL(0, dst[7 < strlen(dst) ? 7 : strlen(dst)]); // NUL inside cap
    TEST_ASSERT_TRUE(strlen(dst) < sizeof(dst));
    TEST_ASSERT_NOT_NULL(strstr(dst, "\xE2\x80\xA6"));
}

void test_clip_four_byte_codepoint() {
    // U+1F600 (4 bytes) counts as one codepoint and is never split.
    char dst[16];
    utf8Clip(dst, sizeof(dst), "\xF0\x9F\x98\x80xy", 1);
    TEST_ASSERT_EQUAL_STRING("\xF0\x9F\x98\x80\xE2\x80\xA6", dst);
}

void test_clip_tiny_cap() {
    char dst[2];
    size_t n = utf8Clip(dst, sizeof(dst), "abc", 3);
    TEST_ASSERT_EQUAL(0, n);                     // cap < 4 → empty string, no overflow
    TEST_ASSERT_EQUAL(0, dst[0]);
}

void test_clip_zero_budget() {
    // max_cp == 0: loop never runs; non-empty src yields just the ellipsis.
    char dst[16];
    utf8Clip(dst, sizeof(dst), "abc", 0);
    TEST_ASSERT_EQUAL_STRING("\xE2\x80\xA6", dst);
    utf8Clip(dst, sizeof(dst), "", 0);
    TEST_ASSERT_EQUAL_STRING("", dst);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_count_ascii);
    RUN_TEST(test_count_cyrillic);
    RUN_TEST(test_count_mixed);
    RUN_TEST(test_advance);
    RUN_TEST(test_clip_no_truncation);
    RUN_TEST(test_clip_truncates_with_ellipsis);
    RUN_TEST(test_clip_exact_fit_no_ellipsis);
    RUN_TEST(test_clip_cap_bound);
    RUN_TEST(test_clip_four_byte_codepoint);
    RUN_TEST(test_clip_tiny_cap);
    RUN_TEST(test_clip_zero_budget);
    return UNITY_END();
}
