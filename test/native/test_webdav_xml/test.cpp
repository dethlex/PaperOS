#include <unity.h>
#include "util/WebDavXml.h"

using namespace paperos;
void setUp() {} void tearDown() {}

static bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

void test_http_date_epoch() {
    TEST_ASSERT_EQUAL_STRING("Thu, 01 Jan 1970 00:00:00 GMT", httpDate(0).c_str());
}
void test_propfind_dir_self_is_collection() {
    std::vector<DavEntry> kids;
    std::string xml = buildPropfindXml("/", true, kids);
    TEST_ASSERT_TRUE(has(xml, "<D:multistatus xmlns:D=\"DAV:\">"));
    TEST_ASSERT_TRUE(has(xml, "<D:collection/>"));
    TEST_ASSERT_TRUE(has(xml, "<D:href>/</D:href>"));
}
void test_propfind_children() {
    std::vector<DavEntry> kids = {
        {"books", true, 0, 0},
        {"a.txt", false, 123, 0},
    };
    std::string xml = buildPropfindXml("/", true, kids);
    TEST_ASSERT_TRUE(has(xml, "<D:href>/books/</D:href>"));        // directory → trailing '/'
    TEST_ASSERT_TRUE(has(xml, "<D:href>/a.txt</D:href>"));
    TEST_ASSERT_TRUE(has(xml, "<D:getcontentlength>123</D:getcontentlength>"));
}
void test_propfind_href_encoding() {
    std::vector<DavEntry> kids = {{"My Book.txt", false, 1, 0}};
    std::string xml = buildPropfindXml("/", true, kids);
    TEST_ASSERT_TRUE(has(xml, "/My%20Book.txt"));
}
void test_lock_has_token() {
    std::string xml = buildLockXml("opaquelocktoken:paperos-1", 3600);
    TEST_ASSERT_TRUE(has(xml, "<D:locktoken><D:href>opaquelocktoken:paperos-1</D:href></D:locktoken>"));
    TEST_ASSERT_TRUE(has(xml, "Second-3600"));
}
void test_propfind_file_self_size() {
    // PROPFIND on a file (Depth:0): the self-response must carry the real size.
    std::vector<DavEntry> none;
    std::string xml = buildPropfindXml("/books/a.txt", false, none, 4096, 0);
    TEST_ASSERT_TRUE(has(xml, "<D:href>/books/a.txt</D:href>"));
    TEST_ASSERT_TRUE(has(xml, "<D:getcontentlength>4096</D:getcontentlength>"));
    TEST_ASSERT_FALSE(has(xml, "<D:collection/>"));
}
void test_proppatch_success() {
    std::string xml = buildProppatchXml("/books/My Book.txt");
    TEST_ASSERT_TRUE(has(xml, "<D:multistatus xmlns:D=\"DAV:\">"));
    TEST_ASSERT_TRUE(has(xml, "<D:href>/books/My%20Book.txt</D:href>"));   // href is encoded
    TEST_ASSERT_TRUE(has(xml, "<D:status>HTTP/1.1 200 OK</D:status>"));
}
void test_propfind_mtime_emits_lastmodified() {
    std::vector<DavEntry> kids = {{"a.txt", false, 1, 0}};
    // self-mtime>0 → <D:getlastmodified> present with an RFC1123 date.
    std::string xml = buildPropfindXml("/", true, kids, 0, 0);
    TEST_ASSERT_FALSE(has(xml, "<D:getlastmodified>"));      // mtime=0 everywhere → absent
    std::string xml2 = buildPropfindXml("/", true, kids, 0, 0);
    std::vector<DavEntry> kids2 = {{"a.txt", false, 1, 1000000000u}};
    std::string xml3 = buildPropfindXml("/", true, kids2, 0, 0);
    TEST_ASSERT_TRUE(has(xml3, "<D:getlastmodified>Sun, 09 Sep 2001 01:46:40 GMT</D:getlastmodified>"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_http_date_epoch);
    RUN_TEST(test_propfind_dir_self_is_collection);
    RUN_TEST(test_propfind_children);
    RUN_TEST(test_propfind_href_encoding);
    RUN_TEST(test_lock_has_token);
    RUN_TEST(test_proppatch_success);
    RUN_TEST(test_propfind_file_self_size);
    RUN_TEST(test_propfind_mtime_emits_lastmodified);
    return UNITY_END();
}
