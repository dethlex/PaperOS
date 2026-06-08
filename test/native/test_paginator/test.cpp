#include <unity.h>
#include "apps/reader/Paginator.h"
#include "apps/reader/EncodingDetect.h"
#include <string>
#include <cstring>

using namespace paperos;

// Mock font metrics: every glyph is `kCharW` pixels wide, line height `kLineH`.
struct MockMetrics : public FontMetrics {
    int kCharW; int kLineH;
    MockMetrics(int w, int h) : kCharW(w), kLineH(h) {}
    int advanceFor(uint32_t cp) const override { (void)cp; return kCharW; }
    int lineHeight() const override { return kLineH; }
};

void setUp() {}
void tearDown() {}

void test_empty_input_yields_one_page() {
    MockMetrics m{ 10, 30 };
    PageBreaker pb({200, 90, 0, &m, Encoding::Utf8});
    auto pages = pb.paginate(reinterpret_cast<const uint8_t*>(""), 0);
    TEST_ASSERT_EQUAL_size_t(1, pages.size());
    TEST_ASSERT_EQUAL_size_t(0, pages[0]);
}

void test_word_wrap_on_space() {
    MockMetrics m{ 10, 30 };
    PageBreaker pb({200, 90, 0, &m, Encoding::Utf8});
    const char* s = "hello world hello world hello";
    auto pages = pb.paginate(reinterpret_cast<const uint8_t*>(s), strlen(s));
    TEST_ASSERT_GREATER_OR_EQUAL(1, pages.size());
}

void test_long_word_breaks_at_width() {
    MockMetrics m{ 10, 30 };
    PageBreaker pb({50, 90, 0, &m, Encoding::Utf8});
    const char* s = "abcdefghijklmno";
    auto pages = pb.paginate(reinterpret_cast<const uint8_t*>(s), strlen(s));
    TEST_ASSERT_GREATER_OR_EQUAL(1, pages.size());
}

void test_newline_breaks_paragraph() {
    MockMetrics m{ 10, 30 };
    PageBreaker pb({200, 90, 0, &m, Encoding::Utf8});
    const char* s = "a\nb\nc\nd\ne";   // 5 paragraphs
    auto pages = pb.paginate(reinterpret_cast<const uint8_t*>(s), strlen(s));
    // 5 lines of 30 px each = 150 px, page is 90 px → at least 2 pages.
    TEST_ASSERT_GREATER_OR_EQUAL(2, pages.size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_input_yields_one_page);
    RUN_TEST(test_word_wrap_on_space);
    RUN_TEST(test_long_word_breaks_at_width);
    RUN_TEST(test_newline_breaks_paragraph);
    return UNITY_END();
}
