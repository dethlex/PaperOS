#include <unity.h>
#include "services/PrinterStatus.h"
#include <cstring>

using namespace paperos;
void setUp() {} void tearDown() {}

static int ri_local(float t) { return static_cast<int>(t < 0 ? t - 0.5f : t + 0.5f); }

// ---------- Task 1: etaRemainingSec ----------

void test_eta_zero_progress() {
    TEST_ASSERT_EQUAL_UINT32(0, etaRemainingSec(1000, 0.f));
}
void test_eta_complete() {
    TEST_ASSERT_EQUAL_UINT32(0, etaRemainingSec(1000, 1.f));
}
void test_eta_half() {
    // 50% in 1000s → ~1000s remaining
    TEST_ASSERT_UINT32_WITHIN(1, 1000, etaRemainingSec(1000, 0.5f));
}
void test_eta_quarter() {
    // 25% in 300s → total 1200s → 900s remaining
    TEST_ASSERT_UINT32_WITHIN(1, 900, etaRemainingSec(300, 0.25f));
}

// ---------- Task 2: parsePrinterStatus ----------

void test_parse_printing() {
    const char* j = R"({"status":{
        "webhooks":{"state":"ready","state_message":"Printer is ready"},
        "print_stats":{"state":"printing","filename":"benchy.gcode",
                       "print_duration":600.0,"info":{"current_layer":12,"total_layer":240}},
        "display_status":{"progress":0.5},
        "virtual_sdcard":{"progress":0.4},
        "extruder":{"temperature":199.8,"target":200.0},
        "heater_bed":{"temperature":59.5,"target":60.0}
    }})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::Klippy::Ready), static_cast<int>(s.klippy));
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::PrintSt::Printing), static_cast<int>(s.state));
    TEST_ASSERT_EQUAL_STRING("benchy.gcode", s.filename.c_str());
    TEST_ASSERT_EQUAL_UINT32(600, s.printDurationS);
    TEST_ASSERT_EQUAL_INT(12, s.currentLayer);
    TEST_ASSERT_EQUAL_INT(240, s.totalLayer);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, s.progress);   // display_status preferred
    TEST_ASSERT_INT_WITHIN(1, 200, (int)(s.hotendTemp + 0.5f));
    TEST_ASSERT_INT_WITHIN(1, 60, (int)(s.bedTarget + 0.5f));
    TEST_ASSERT_FALSE(s.lightKnown);
}
void test_parse_idle_missing_fields() {
    const char* j = R"({"status":{
        "webhooks":{"state":"ready"},
        "print_stats":{"state":"standby"}
    }})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::PrintSt::Standby), static_cast<int>(s.state));
    TEST_ASSERT_EQUAL_STRING("", s.filename.c_str());
    TEST_ASSERT_EQUAL_INT(-1, s.currentLayer);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.f, s.progress);
}
void test_parse_states() {
    auto stateOf = [](const char* st) {
        std::string j = std::string("{\"status\":{\"print_stats\":{\"state\":\"") + st + "\"}}}";
        PrinterStatus s;
        TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
        return s.state;
    };
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::PrintSt::Paused),    static_cast<int>(stateOf("paused")));
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::PrintSt::Complete),  static_cast<int>(stateOf("complete")));
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::PrintSt::Cancelled), static_cast<int>(stateOf("cancelled")));
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::PrintSt::Error),     static_cast<int>(stateOf("error")));
}
void test_parse_progress_fallback_vsd() {
    // no display_status → fall back to virtual_sdcard
    const char* j = R"({"status":{"virtual_sdcard":{"progress":0.33}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.33f, s.progress);
}
void test_parse_progress_prefers_vsd_when_display_zero() {
    // display_status present but 0.0 (no M73) → use virtual_sdcard's real fraction
    const char* j = R"({"status":{
        "display_status":{"progress":0.0},
        "virtual_sdcard":{"progress":0.4}
    }})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4f, s.progress);
}
void test_progress_stale_m73_ignored_during_heatup() {
    // Previous print's M73 (15%) lingers while the new print heats up
    // (print_duration == 0) — must fall back to virtual_sdcard.
    const char* j = R"({"result":{"status":{
        "print_stats":{"state":"printing","filename":"new.gcode","print_duration":0.0},
        "display_status":{"progress":0.15},
        "virtual_sdcard":{"progress":0.001}
    }}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.001f, s.progress);
}
void test_progress_m73_trusted_once_printing() {
    const char* j = R"({"result":{"status":{
        "print_stats":{"state":"printing","filename":"new.gcode","print_duration":120.0},
        "display_status":{"progress":0.15},
        "virtual_sdcard":{"progress":0.10}
    }}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.15f, s.progress);
}
void test_parse_klippy_shutdown() {
    const char* j = R"({"status":{"webhooks":{"state":"shutdown","state_message":"MCU error"}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::Klippy::Shutdown), static_cast<int>(s.klippy));
    TEST_ASSERT_EQUAL_STRING("MCU error", s.klippyMessage.c_str());
}
void test_parse_light_on() {
    const char* j = R"({"status":{"led LED_Light":{"color_data":[[0,0,0,0.8]]}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "led LED_Light", s));
    TEST_ASSERT_TRUE(s.lightKnown);
    TEST_ASSERT_TRUE(s.lightOn);
}
void test_parse_light_off() {
    const char* j = R"({"status":{"led LED_Light":{"color_data":[[0,0,0,0]]}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "led LED_Light", s));
    TEST_ASSERT_TRUE(s.lightKnown);
    TEST_ASSERT_FALSE(s.lightOn);
}
void test_parse_light_rgb_on() {
    // RGB without white channel still reads "on" via any channel > 0
    const char* j = R"({"status":{"neopixel LED_Light":{"color_data":[[0.5,0,0,0]]}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "neopixel LED_Light", s));
    TEST_ASSERT_TRUE(s.lightOn);
}
void test_parse_bad_json_false() {
    PrinterStatus s; s.filename = "keep";
    TEST_ASSERT_FALSE(parsePrinterStatus("{not json", "", s));
    TEST_ASSERT_EQUAL_STRING("keep", s.filename.c_str());
}

// ---------- Task 3: sameForDisplay ----------

void test_same_ignores_subminute_and_subpercent() {
    PrinterStatus a; a.state = PrinterStatus::PrintSt::Printing;
    a.progress = 0.499f; a.printDurationS = 600; a.hotendTemp = 199.6f;
    PrinterStatus b = a;
    b.progress = 0.5049f;          // same 50%
    b.printDurationS = 619;        // same 10 min
    b.hotendTemp = 200.2f;         // same 200 °C
    TEST_ASSERT_TRUE(sameForDisplay(a, b));
}
void test_same_detects_state_change() {
    PrinterStatus a; a.state = PrinterStatus::PrintSt::Printing;
    PrinterStatus b = a; b.state = PrinterStatus::PrintSt::Paused;
    TEST_ASSERT_FALSE(sameForDisplay(a, b));
}
void test_same_detects_light_change() {
    PrinterStatus a; a.lightKnown = true; a.lightOn = false;
    PrinterStatus b = a; b.lightOn = true;
    TEST_ASSERT_FALSE(sameForDisplay(a, b));
}
void test_same_detects_percent_change() {
    PrinterStatus a; a.progress = 0.50f;
    PrinterStatus b = a; b.progress = 0.52f;
    TEST_ASSERT_FALSE(sameForDisplay(a, b));
}

// ---------- Task 4: findLightObject ----------

void test_findlight_led_prefix() {
    const char* j = R"({"objects":["webhooks","toolhead","led LED_Light","extruder"]})";
    TEST_ASSERT_EQUAL_STRING("led LED_Light", findLightObject(j, "LED_Light").c_str());
}
void test_findlight_prefers_led_over_neopixel() {
    const char* j = R"({"objects":["neopixel LED_Light","led LED_Light"]})";
    TEST_ASSERT_EQUAL_STRING("led LED_Light", findLightObject(j, "LED_Light").c_str());
}
void test_findlight_neopixel_only() {
    const char* j = R"({"objects":["neopixel LED_Light","toolhead"]})";
    TEST_ASSERT_EQUAL_STRING("neopixel LED_Light", findLightObject(j, "LED_Light").c_str());
}
void test_findlight_not_found() {
    const char* j = R"({"objects":["webhooks","toolhead"]})";
    TEST_ASSERT_EQUAL_STRING("", findLightObject(j, "LED_Light").c_str());
}
void test_findlight_empty_name() {
    const char* j = R"({"objects":["led LED_Light"]})";
    TEST_ASSERT_EQUAL_STRING("", findLightObject(j, "").c_str());
}
void test_findlight_bad_json() {
    TEST_ASSERT_EQUAL_STRING("", findLightObject("nope", "LED_Light").c_str());
}

// ---------- Task 5: PrinterCache ----------

void test_cache_roundtrip() {
    PrinterStatus a;
    a.klippy = PrinterStatus::Klippy::Ready;
    a.state  = PrinterStatus::PrintSt::Printing;
    a.progress = 0.42f; a.printDurationS = 1234;
    a.currentLayer = 5; a.totalLayer = 100;
    a.hotendTemp = 199.6f; a.hotendTarget = 200.f;
    a.bedTemp = 59.4f; a.bedTarget = 60.f;
    a.lightOn = true; a.lightKnown = true;
    PrinterCache c = toCache(a);
    PrinterStatus b = fromCache(c);
    TEST_ASSERT_EQUAL(static_cast<int>(a.klippy), static_cast<int>(b.klippy));
    TEST_ASSERT_EQUAL(static_cast<int>(a.state), static_cast<int>(b.state));
    TEST_ASSERT_EQUAL_INT(5, b.currentLayer);
    TEST_ASSERT_EQUAL_INT(100, b.totalLayer);
    TEST_ASSERT_INT_WITHIN(1, 200, ri_local(b.hotendTemp));
    TEST_ASSERT_TRUE(b.lightOn);
    TEST_ASSERT_TRUE(b.lightKnown);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.42f, b.progress);
}
void test_cache_bad_version_defaults() {
    PrinterCache c{}; c.version = 99; c.state = 1;
    PrinterStatus b = fromCache(c);
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::PrintSt::Unknown), static_cast<int>(b.state));
}

// Moonraker's HTTP REST API wraps every payload in {"result": {...}}.
// The status / objects live under "result", not at the document root.
void test_parse_result_envelope() {
    const char* j = R"({"result":{"status":{
        "print_stats":{"state":"printing","print_duration":600.0},
        "display_status":{"progress":0.5},
        "extruder":{"temperature":200.0,"target":200.0}
    }}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_EQUAL(static_cast<int>(PrinterStatus::PrintSt::Printing), static_cast<int>(s.state));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, s.progress);
    TEST_ASSERT_INT_WITHIN(1, 200, (int)(s.hotendTemp + 0.5f));
}
void test_parse_result_envelope_light() {
    const char* j = R"({"result":{"status":{"led LED_Light":{"color_data":[[0,0,0,1.0]]}}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "led LED_Light", s));
    TEST_ASSERT_TRUE(s.lightKnown);
    TEST_ASSERT_TRUE(s.lightOn);
}
void test_findlight_result_envelope() {
    const char* j = R"({"result":{"objects":["webhooks","led LED_Light","toolhead"]}})";
    TEST_ASSERT_EQUAL_STRING("led LED_Light", findLightObject(j, "LED_Light").c_str());
}

// ---------- Task 1: homedAxes ----------
void test_homed_axes_parsed() {
    const char* j = R"({"status":{
        "toolhead":{"homed_axes":"xyz"},
        "print_stats":{"state":"standby"},
        "webhooks":{"state":"ready"}
    }})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_EQUAL_STRING("xyz", s.homedAxes.c_str());
}
void test_homed_axes_partial() {
    const char* j = R"({"status":{"toolhead":{"homed_axes":"x"}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_EQUAL_STRING("x", s.homedAxes.c_str());
}
void test_homed_axes_absent_is_empty() {
    const char* j = R"({"status":{"print_stats":{"state":"standby"}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_EQUAL_STRING("", s.homedAxes.c_str());
}
void test_homed_axes_in_sameForDisplay() {
    PrinterStatus a; a.homedAxes = "xyz";
    PrinterStatus b; b.homedAxes = "";
    TEST_ASSERT_FALSE(sameForDisplay(a, b));
    b.homedAxes = "xyz";
    TEST_ASSERT_TRUE(sameForDisplay(a, b));
}

// ---------- Task 2: URL builders ----------
void test_url_encode_keeps_unreserved() {
    TEST_ASSERT_EQUAL_STRING("Abc_0.2-x~/y", urlEncode("Abc_0.2-x~/y").c_str());
}
void test_url_encode_escapes_specials() {
    TEST_ASSERT_EQUAL_STRING("a%20b%23c", urlEncode("a b#c").c_str());
}
void test_power_url_is_bare_key_not_device_param() {
    std::string on = buildPowerUrl("http://p", "sonoff", true);
    TEST_ASSERT_EQUAL_STRING("http://p/machine/device_power/on?sonoff", on.c_str());
    std::string off = buildPowerUrl("http://p", "sonoff", false);
    TEST_ASSERT_EQUAL_STRING("http://p/machine/device_power/off?sonoff", off.c_str());
    // regression guard: must NOT use ?device=
    TEST_ASSERT_NULL(strstr(on.c_str(), "device="));
}
void test_power_status_url_uses_device_param() {
    TEST_ASSERT_EQUAL_STRING("http://p/machine/device_power/device?device=sonoff",
        buildPowerStatusUrl("http://p", "sonoff").c_str());
}
void test_print_start_url_encodes_filename() {
    TEST_ASSERT_EQUAL_STRING("http://p/printer/print/start?filename=My%20Part_0.2mm.gcode",
        buildPrintStartUrl("http://p", "My Part_0.2mm.gcode").c_str());
}

// ---------- Task 3: gcode builders ----------
void test_jog_positive() {
    TEST_ASSERT_EQUAL_STRING("G91\nG1 X10 F3000\nG90", buildJogScript('X', 10, 3000).c_str());
}
void test_jog_negative() {
    TEST_ASSERT_EQUAL_STRING("G91\nG1 Z-10 F600\nG90", buildJogScript('Z', -10, 600).c_str());
}
void test_preheat_script() {
    TEST_ASSERT_EQUAL_STRING("M104 S200\nM140 S60", buildPreheatScript(200, 60).c_str());
}

// ---------- jsonEscape ----------
void test_json_escape_newline() {
    // buildJogScript produces real newlines; jsonEscape must turn them to \n (backslash-n)
    TEST_ASSERT_EQUAL_STRING("G91\\nG1 X10 F3000\\nG90",
        jsonEscape("G91\nG1 X10 F3000\nG90").c_str());
}
void test_json_escape_quote_backslash() {
    TEST_ASSERT_EQUAL_STRING("a\\\"b\\\\c", jsonEscape("a\"b\\c").c_str());
}
void test_json_escape_tab_and_control() {
    TEST_ASSERT_EQUAL_STRING("x\\ty\\u0001z", jsonEscape("x\ty\x01z").c_str());
}
void test_json_escape_plain() {
    TEST_ASSERT_EQUAL_STRING("plain", jsonEscape("plain").c_str());
}

// ---------- Redesign: parseFirstPowerDevice ----------
void test_first_power_device() {
    const char* j = R"({"result":{"devices":[
        {"device":"sonoff","status":"off","type":"http"},
        {"device":"psu2","status":"on"}
    ]}})";
    TEST_ASSERT_EQUAL_STRING("sonoff", parseFirstPowerDevice(j).c_str());
}
void test_first_power_device_empty_list() {
    TEST_ASSERT_EQUAL_STRING("", parseFirstPowerDevice(R"({"result":{"devices":[]}})").c_str());
}
void test_first_power_device_bare_top() {
    TEST_ASSERT_EQUAL_STRING("s", parseFirstPowerDevice(R"({"devices":[{"device":"s"}]})").c_str());
}
void test_first_power_device_bad_json() {
    TEST_ASSERT_EQUAL_STRING("", parseFirstPowerDevice("nope").c_str());
}

// ---------- Task 4: parsePowerState ----------
void test_power_on() {
    TEST_ASSERT_EQUAL(static_cast<int>(PowerState::On),
        static_cast<int>(parsePowerState(R"({"result":{"sonoff":"on"}})", "sonoff")));
}
void test_power_off() {
    TEST_ASSERT_EQUAL(static_cast<int>(PowerState::Off),
        static_cast<int>(parsePowerState(R"({"result":{"sonoff":"off"}})", "sonoff")));
}
void test_power_unknown_device_missing() {
    TEST_ASSERT_EQUAL(static_cast<int>(PowerState::Unknown),
        static_cast<int>(parsePowerState(R"({"result":{"other":"on"}})", "sonoff")));
}
void test_power_bare_top_fallback() {
    TEST_ASSERT_EQUAL(static_cast<int>(PowerState::On),
        static_cast<int>(parsePowerState(R"({"sonoff":"on"})", "sonoff")));
}
void test_power_bad_json() {
    TEST_ASSERT_EQUAL(static_cast<int>(PowerState::Unknown),
        static_cast<int>(parsePowerState("not json", "sonoff")));
}

// ---------- Task 5: history ----------
void test_clean_name_keeps_slicer_suffix() {
    TEST_ASSERT_EQUAL_STRING("3DBenchy 0.2mm PLA NEPTUNE3PRO 1h32m",
        cleanDisplayName("3DBenchy_0.2mm_PLA_NEPTUNE3PRO_1h32m.gcode").c_str());
}
void test_clean_name_underscores_to_spaces() {
    TEST_ASSERT_EQUAL_STRING("Air Tag Holder Base 0.2mm PLA NEPTUNE3PRO 41m",
        cleanDisplayName("Air_Tag_Holder_Base_0.2mm_PLA_NEPTUNE3PRO_41m.gcode").c_str());
}
void test_clean_name_double_underscore_collapsed() {
    TEST_ASSERT_EQUAL_STRING("260PETG Temp Tower 0.16mm PETG NEPTUNE3PRO 1h28m",
        cleanDisplayName("260PETG_Temp_Tower__0.16mm_PETG_NEPTUNE3PRO_1h28m.gcode").c_str());
}
void test_clean_name_strips_dir_and_ext() {
    TEST_ASSERT_EQUAL_STRING("Foo 0.16mm PETG", cleanDisplayName("subdir/Foo_0.16mm_PETG.gcode").c_str());
}
void test_clean_name_plain() {
    TEST_ASSERT_EQUAL_STRING("plain", cleanDisplayName("plain.gcode").c_str());
}
void test_clean_name_no_ext() {
    TEST_ASSERT_EQUAL_STRING("NoExtName", cleanDisplayName("NoExtName").c_str());
}
void test_history_filters_exists_and_dedups() {
    const char* j = R"({"result":{"jobs":[
        {"filename":"a_0.2mm_PLA.gcode","exists":true,"end_time":1700000001},
        {"filename":"gone_0.2mm.gcode","exists":false,"end_time":1700000002},
        {"filename":"a_0.2mm_PLA.gcode","exists":true,"end_time":1700000003},
        {"filename":"b_0.2mm_PLA.gcode","exists":true,"end_time":1700000004}
    ]}})";
    auto v = parseHistory(j, 10);
    TEST_ASSERT_EQUAL_UINT32(2, v.size());
    TEST_ASSERT_EQUAL_STRING("a_0.2mm_PLA.gcode", v[0].filename.c_str());
    TEST_ASSERT_EQUAL_STRING("a 0.2mm PLA", v[0].displayName.c_str());
    TEST_ASSERT_EQUAL_STRING("b_0.2mm_PLA.gcode", v[1].filename.c_str());
}
void test_history_respects_limit() {
    const char* j = R"({"result":{"jobs":[
        {"filename":"a.gcode","exists":true,"end_time":1700000000},
        {"filename":"b.gcode","exists":true,"end_time":1700000000},
        {"filename":"c.gcode","exists":true,"end_time":1700000000}
    ]}})";
    TEST_ASSERT_EQUAL_UINT32(2, parseHistory(j, 2).size());
}
void test_history_bad_json_empty() {
    TEST_ASSERT_EQUAL_UINT32(0, parseHistory("nope", 10).size());
}
void test_history_lastprint_end_preferred() {
    const char* j = R"({"result":{"jobs":[
        {"filename":"a_0.2mm.gcode","exists":true,"start_time":100,"end_time":200}
    ]}})";
    auto v = parseHistory(j, 10);
    TEST_ASSERT_EQUAL_UINT32(1, v.size());
    TEST_ASSERT_EQUAL_UINT32(200, v[0].lastPrint);
}
void test_history_lastprint_falls_back_to_start() {
    const char* j = R"({"result":{"jobs":[
        {"filename":"a.gcode","exists":true,"start_time":150}
    ]}})";
    auto v = parseHistory(j, 10);
    TEST_ASSERT_EQUAL_UINT32(1, v.size());
    TEST_ASSERT_EQUAL_UINT32(150, v[0].lastPrint);
}
void test_history_skips_entry_without_time() {
    // Most-recent job for X has no time -> X hidden entirely (older timed job must NOT appear).
    const char* j = R"({"result":{"jobs":[
        {"filename":"X.gcode","exists":true},
        {"filename":"X.gcode","exists":true,"end_time":200},
        {"filename":"Y.gcode","exists":true,"end_time":300}
    ]}})";
    auto v = parseHistory(j, 10);
    TEST_ASSERT_EQUAL_UINT32(1, v.size());
    TEST_ASSERT_EQUAL_STRING("Y.gcode", v[0].filename.c_str());
}
void test_format_print_time() {
    char b[20];
    formatPrintTime(0, 0, b, sizeof(b));     TEST_ASSERT_EQUAL_STRING("01.01 00:00", b);
    formatPrintTime(3600, 0, b, sizeof(b));  TEST_ASSERT_EQUAL_STRING("01.01 01:00", b);
    formatPrintTime(0, 3, b, sizeof(b));     TEST_ASSERT_EQUAL_STRING("01.01 03:00", b);
    formatPrintTime(86400, 0, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING("02.01 00:00", b);
}

// ---------- Slicer estimate ----------
void test_slicer_estimate_parsed() {
    TEST_ASSERT_EQUAL_UINT32(12485, parseSlicerEstimate(R"({"result":{"estimated_time":12485.0}})"));
}
void test_slicer_estimate_bare_top() {
    TEST_ASSERT_EQUAL_UINT32(3600, parseSlicerEstimate(R"({"estimated_time":3600})"));
}
void test_slicer_estimate_absent_zero() {
    TEST_ASSERT_EQUAL_UINT32(0, parseSlicerEstimate(R"({"result":{"foo":1}})"));
}
void test_slicer_estimate_bad_json_zero() {
    TEST_ASSERT_EQUAL_UINT32(0, parseSlicerEstimate("nope"));
}
void test_metadata_url_encodes() {
    TEST_ASSERT_EQUAL_STRING("http://p/server/files/metadata?filename=a%20b.gcode",
        buildMetadataUrl("http://p", "a b.gcode").c_str());
}

// ---------- Redesign: thumbnail path/url ----------
void test_thumbnail_picks_largest() {
    const char* j = R"({"result":{"thumbnails":[
        {"width":32,"height":32,"relative_path":".thumbs/a-32x32.png"},
        {"width":220,"height":124,"relative_path":".thumbs/a-220x124.png"},
        {"width":16,"height":16,"relative_path":".thumbs/a-16x16.png"}
    ]}})";
    TEST_ASSERT_EQUAL_STRING(".thumbs/a-220x124.png", parseThumbnailPath(j).c_str());
}
void test_thumbnail_absent_empty() {
    TEST_ASSERT_EQUAL_STRING("", parseThumbnailPath(R"({"result":{"estimated_time":1}})").c_str());
}
void test_thumbnail_bad_json_empty() {
    TEST_ASSERT_EQUAL_STRING("", parseThumbnailPath("nope").c_str());
}
void test_thumbnail_url_root_file() {
    TEST_ASSERT_EQUAL_STRING("http://p/server/files/gcodes/.thumbs/a-220x124.png",
        buildThumbnailUrl("http://p", "a.gcode", ".thumbs/a-220x124.png").c_str());
}
void test_thumbnail_url_subdir_and_encode() {
    TEST_ASSERT_EQUAL_STRING("http://p/server/files/gcodes/sub%20dir/.thumbs/b-220x124.png",
        buildThumbnailUrl("http://p", "sub dir/b.gcode", ".thumbs/b-220x124.png").c_str());
}

// ---------- File-relative progress (Fluidd-style byte range) ----------
void test_gcode_byte_range_parsed() {
    uint32_t s = 0, e = 0;
    TEST_ASSERT_TRUE(parseGcodeByteRange(
        R"({"result":{"gcode_start_byte":9920,"gcode_end_byte":8209510}})", s, e));
    TEST_ASSERT_EQUAL_UINT32(9920, s);
    TEST_ASSERT_EQUAL_UINT32(8209510, e);
}
void test_gcode_byte_range_missing_or_degenerate() {
    uint32_t s = 0, e = 0;
    TEST_ASSERT_FALSE(parseGcodeByteRange(R"({"result":{"estimated_time":1}})", s, e));
    TEST_ASSERT_FALSE(parseGcodeByteRange(R"({"result":{"gcode_start_byte":100,"gcode_end_byte":100}})", s, e));
    TEST_ASSERT_FALSE(parseGcodeByteRange("nope", s, e));
}
void test_file_relative_progress() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.f, fileRelativeProgress(10847, 9920, 8209510));  // heat-up ≈ 0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.f, fileRelativeProgress(5000, 9920, 8209510));   // before start
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.f, fileRelativeProgress(9000000, 9920, 8209510)); // past end
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, fileRelativeProgress(150, 100, 200));
}
void test_file_position_parsed() {
    const char* j = R"({"result":{"status":{"virtual_sdcard":{"progress":0.14,"file_position":10847}}}})";
    PrinterStatus s;
    TEST_ASSERT_TRUE(parsePrinterStatus(j, "", s));
    TEST_ASSERT_EQUAL_UINT32(10847, s.filePosition);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_first_power_device);
    RUN_TEST(test_first_power_device_empty_list);
    RUN_TEST(test_first_power_device_bare_top);
    RUN_TEST(test_first_power_device_bad_json);
    RUN_TEST(test_eta_zero_progress);
    RUN_TEST(test_eta_complete);
    RUN_TEST(test_eta_half);
    RUN_TEST(test_eta_quarter);
    RUN_TEST(test_parse_printing);
    RUN_TEST(test_parse_idle_missing_fields);
    RUN_TEST(test_parse_states);
    RUN_TEST(test_parse_progress_fallback_vsd);
    RUN_TEST(test_parse_progress_prefers_vsd_when_display_zero);
    RUN_TEST(test_progress_stale_m73_ignored_during_heatup);
    RUN_TEST(test_progress_m73_trusted_once_printing);
    RUN_TEST(test_parse_klippy_shutdown);
    RUN_TEST(test_parse_light_on);
    RUN_TEST(test_parse_light_off);
    RUN_TEST(test_parse_light_rgb_on);
    RUN_TEST(test_parse_bad_json_false);
    RUN_TEST(test_same_ignores_subminute_and_subpercent);
    RUN_TEST(test_same_detects_state_change);
    RUN_TEST(test_same_detects_light_change);
    RUN_TEST(test_same_detects_percent_change);
    RUN_TEST(test_findlight_led_prefix);
    RUN_TEST(test_findlight_prefers_led_over_neopixel);
    RUN_TEST(test_findlight_neopixel_only);
    RUN_TEST(test_findlight_not_found);
    RUN_TEST(test_findlight_empty_name);
    RUN_TEST(test_findlight_bad_json);
    RUN_TEST(test_cache_roundtrip);
    RUN_TEST(test_cache_bad_version_defaults);
    RUN_TEST(test_parse_result_envelope);
    RUN_TEST(test_parse_result_envelope_light);
    RUN_TEST(test_findlight_result_envelope);
    RUN_TEST(test_homed_axes_parsed);
    RUN_TEST(test_homed_axes_partial);
    RUN_TEST(test_homed_axes_absent_is_empty);
    RUN_TEST(test_homed_axes_in_sameForDisplay);
    RUN_TEST(test_url_encode_keeps_unreserved);
    RUN_TEST(test_url_encode_escapes_specials);
    RUN_TEST(test_power_url_is_bare_key_not_device_param);
    RUN_TEST(test_power_status_url_uses_device_param);
    RUN_TEST(test_print_start_url_encodes_filename);
    RUN_TEST(test_jog_positive);
    RUN_TEST(test_jog_negative);
    RUN_TEST(test_preheat_script);
    RUN_TEST(test_json_escape_newline);
    RUN_TEST(test_json_escape_quote_backslash);
    RUN_TEST(test_json_escape_tab_and_control);
    RUN_TEST(test_json_escape_plain);
    RUN_TEST(test_power_on);
    RUN_TEST(test_power_off);
    RUN_TEST(test_power_unknown_device_missing);
    RUN_TEST(test_power_bare_top_fallback);
    RUN_TEST(test_power_bad_json);
    RUN_TEST(test_clean_name_keeps_slicer_suffix);
    RUN_TEST(test_clean_name_underscores_to_spaces);
    RUN_TEST(test_clean_name_double_underscore_collapsed);
    RUN_TEST(test_clean_name_strips_dir_and_ext);
    RUN_TEST(test_clean_name_plain);
    RUN_TEST(test_clean_name_no_ext);
    RUN_TEST(test_history_filters_exists_and_dedups);
    RUN_TEST(test_history_respects_limit);
    RUN_TEST(test_history_bad_json_empty);
    RUN_TEST(test_history_lastprint_end_preferred);
    RUN_TEST(test_history_lastprint_falls_back_to_start);
    RUN_TEST(test_history_skips_entry_without_time);
    RUN_TEST(test_format_print_time);
    RUN_TEST(test_slicer_estimate_parsed);
    RUN_TEST(test_slicer_estimate_bare_top);
    RUN_TEST(test_slicer_estimate_absent_zero);
    RUN_TEST(test_slicer_estimate_bad_json_zero);
    RUN_TEST(test_metadata_url_encodes);
    RUN_TEST(test_thumbnail_picks_largest);
    RUN_TEST(test_thumbnail_absent_empty);
    RUN_TEST(test_thumbnail_bad_json_empty);
    RUN_TEST(test_thumbnail_url_root_file);
    RUN_TEST(test_thumbnail_url_subdir_and_encode);
    RUN_TEST(test_gcode_byte_range_parsed);
    RUN_TEST(test_gcode_byte_range_missing_or_degenerate);
    RUN_TEST(test_file_relative_progress);
    RUN_TEST(test_file_position_parsed);
    return UNITY_END();
}
