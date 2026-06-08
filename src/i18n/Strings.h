#pragma once
#include <cstdint>

namespace paperos {

enum class Lang : uint8_t { Ru = 0, En = 1 };

// UI string ids. Order MUST match kRu[]/kEn[] in lang_ru.cpp/lang_en.cpp.
// _Count is the completeness sentinel. (common_back is reserved — no on-screen
// Back today; G38 is the back button.)
enum class Str : uint16_t {
    app_home, app_reader, app_ha, app_weather, app_games, app_settings,
    app_screensaver, app_files, app_fileserver,
    game_tictactoe, game_minesweeper, game_fifteen, game_sudoku,
    common_new_game, common_back, common_no_sd,
    launcher_sd_unavailable, launcher_need_sd,
    ttt_you_won, ttt_x_won, ttt_ai_won, ttt_o_won, ttt_draw, ttt_your_move,
    ttt_move_x, ttt_move_o, ttt_vs_ai, ttt_vs_human,
    mines_win, mines_boom, mines_status_fmt, mines_flag, mines_open,
    fifteen_solved_fmt, fifteen_moves_fmt,
    sudoku_solved, sudoku_erase,
    ha_no_dashboard, ha_invalid, ha_locked, ha_unlocked,
    reader_no_books, reader_page_fmt,
    weather_offline, weather_no_data,
    fs_open_on_computer, fs_finder_hint, fs_root, fs_back_stop, fs_insert_card,
    fs_wifi_off, fs_wifi_settings, fs_connecting, fs_connect_fail, fs_check_wifi,
    fs_server_fail,
    boot_loading,
    set_reading, set_time, set_about, set_password, set_hide_password,
    set_show_password, set_test, set_hide_token, set_show_token, set_check,
    set_font_size, set_margin, set_sync_ntp, set_sec_fmt, set_idle_to_saver,
    set_min_fmt, set_photo_rotate, set_lat, set_lon, set_refresh_interval,
    set_temp_offset, set_update_now, set_version, set_sync_config, set_reboot,
    set_connecting, set_error_fmt, set_checking_ha, set_wifi_fail, set_ha_ok_fmt,
    set_auth_rejected, set_endpoint_404, set_updating_weather, set_wx_ok_fmt,
    set_wx_fail, set_time_updated, set_ntp_fail, set_synced, set_json_error,
    set_language,
    wmo_clear, wmo_partly, wmo_cloudy, wmo_fog, wmo_rain, wmo_snow, wmo_thunder,
    wmo_unknown,
    weather_updated_fmt,
    _Count
};

const char* tr(Str id);            // active-language string; out-of-range → safe clamp to slot 0
void        i18nSetLang(Lang l);   // set active language (RAM)
Lang        i18nLang();            // current active language
const char* const* monthsShort();  // 12 short month names, active language (Jan-first)
const char* const* daysShort();    // 7 short weekday names, active language (Sun-first)

} // namespace paperos
