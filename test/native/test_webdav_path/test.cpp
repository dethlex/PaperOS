#include <unity.h>
#include "util/WebDavPath.h"

using namespace paperos;
void setUp() {} void tearDown() {}

void test_root() {
    std::string out;
    TEST_ASSERT_TRUE(mapUrlToSdPath("/", out));
    TEST_ASSERT_EQUAL_STRING("/paperos", out.c_str());
}
void test_simple() {
    std::string out;
    TEST_ASSERT_TRUE(mapUrlToSdPath("/books/x.txt", out));
    TEST_ASSERT_EQUAL_STRING("/paperos/books/x.txt", out.c_str());
}
void test_percent_decode() {
    std::string out;
    TEST_ASSERT_TRUE(mapUrlToSdPath("/books/My%20Book.txt", out));
    TEST_ASSERT_EQUAL_STRING("/paperos/books/My Book.txt", out.c_str());
}
void test_traversal_plain() {
    std::string out;
    TEST_ASSERT_FALSE(mapUrlToSdPath("/../config.json", out));
}
void test_traversal_mid() {
    std::string out;
    TEST_ASSERT_FALSE(mapUrlToSdPath("/books/../../etc", out));
}
void test_traversal_encoded() {
    std::string out;
    TEST_ASSERT_FALSE(mapUrlToSdPath("/%2e%2e/secret", out));
}
void test_collapse_slashes() {
    std::string out;
    TEST_ASSERT_TRUE(mapUrlToSdPath("//books///x.txt", out));
    TEST_ASSERT_EQUAL_STRING("/paperos/books/x.txt", out.c_str());
}
void test_dot_segment_ok() {
    std::string out;
    TEST_ASSERT_TRUE(mapUrlToSdPath("/books/./x.txt", out));
    TEST_ASSERT_EQUAL_STRING("/paperos/books/x.txt", out.c_str());
}
void test_urldecode() {
    TEST_ASSERT_EQUAL_STRING("a b",  urlDecode("a%20b").c_str());
    TEST_ASSERT_EQUAL_STRING("a b",  urlDecode("a b").c_str());
    TEST_ASSERT_EQUAL_STRING("%",    urlDecode("%").c_str());   // truncated → literal
    TEST_ASSERT_EQUAL_STRING("%zz",  urlDecode("%zz").c_str()); // malformed hex → literal
}
void test_urlencode() {
    TEST_ASSERT_EQUAL_STRING("My%20Book.txt", urlEncode("My Book.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("/books/a.txt",  urlEncode("/books/a.txt").c_str());
}
void test_urlencode_high_byte() {
    // 0xD0 0xB0 = UTF-8 for Cyrillic 'а'. The encoder takes unsigned char →
    // %D0%B0, without sign-extension into %FFFFFF...
    std::string in; in.push_back((char)0xD0); in.push_back((char)0xB0);
    TEST_ASSERT_EQUAL_STRING("%D0%B0", urlEncode(in).c_str());
}
void test_outpath_cleared_on_reject() {
    std::string out = "грязь";
    TEST_ASSERT_FALSE(mapUrlToSdPath("/../x", out));
    TEST_ASSERT_EQUAL_STRING("", out.c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_root);
    RUN_TEST(test_simple);
    RUN_TEST(test_percent_decode);
    RUN_TEST(test_traversal_plain);
    RUN_TEST(test_traversal_mid);
    RUN_TEST(test_traversal_encoded);
    RUN_TEST(test_collapse_slashes);
    RUN_TEST(test_dot_segment_ok);
    RUN_TEST(test_urldecode);
    RUN_TEST(test_urlencode);
    RUN_TEST(test_urlencode_high_byte);
    RUN_TEST(test_outpath_cleared_on_reject);
    return UNITY_END();
}
