#include <unity.h>
#include "apps/ha/Dashboard.h"

using namespace paperos;

void setUp() {}
void tearDown() {}

void test_parse_valid_dashboard() {
    const char* j = R"({
      "groups": [
        {"title": "Kitchen", "items": [
          {"entity_id":"light.kitchen","kind":"toggle","label":"Light"},
          {"entity_id":"sensor.temp","kind":"sensor","label":"T","unit":"°C"},
          {"entity_id":"script.boil","kind":"action","label":"Boil"}
        ]}
      ]
    })";
    Dashboard d;
    TEST_ASSERT_TRUE(d.parse(j));
    TEST_ASSERT_EQUAL_size_t(1, d.groups().size());
    const auto& g = d.groups()[0];
    TEST_ASSERT_EQUAL_STRING("Kitchen", g.title.c_str());
    TEST_ASSERT_EQUAL_size_t(3, g.items.size());
    TEST_ASSERT_EQUAL(static_cast<int>(WidgetKind::Toggle), static_cast<int>(g.items[0].kind));
    TEST_ASSERT_EQUAL(static_cast<int>(WidgetKind::Sensor), static_cast<int>(g.items[1].kind));
    TEST_ASSERT_EQUAL(static_cast<int>(WidgetKind::Action), static_cast<int>(g.items[2].kind));
}

void test_invalid_json_returns_false() {
    Dashboard d;
    TEST_ASSERT_FALSE(d.parse("not json"));
    TEST_ASSERT_EQUAL_size_t(0, d.groups().size());
}

void test_missing_groups_returns_false() {
    Dashboard d;
    TEST_ASSERT_FALSE(d.parse("{}"));
}

void test_unknown_kind_skipped() {
    const char* j = R"({"groups":[{"title":"g","items":[{"entity_id":"x","kind":"weird"}]}]})";
    Dashboard d;
    TEST_ASSERT_TRUE(d.parse(j));
    TEST_ASSERT_EQUAL_size_t(0, d.groups()[0].items.size());
}

void test_lock_kind_parsed() {
    const char* j = R"({"groups":[{"title":"Sec","items":[
        {"entity_id":"lock.front","kind":"lock","label":"Door"}
    ]}]})";
    Dashboard d;
    TEST_ASSERT_TRUE(d.parse(j));
    TEST_ASSERT_EQUAL_size_t(1, d.groups()[0].items.size());
    TEST_ASSERT_EQUAL(static_cast<int>(WidgetKind::Lock),
                      static_cast<int>(d.groups()[0].items[0].kind));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_valid_dashboard);
    RUN_TEST(test_invalid_json_returns_false);
    RUN_TEST(test_missing_groups_returns_false);
    RUN_TEST(test_unknown_kind_skipped);
    RUN_TEST(test_lock_kind_parsed);
    return UNITY_END();
}
