#include "i18n/Strings.h"
#include <cstddef>

// IMPORTANT: array order MUST match `enum class Str` in Strings.h (positional lookup).
// The static_assert below catches COUNT drift but NOT a same-count reorder — when adding
// a Str enumerator, insert the matching string at the SAME position here.
// Master string table: docs/superpowers/plans/2026-06-06-i18n.md

namespace paperos {

extern const char* const kEn[] = {
    "Home", "Books", "Smart Home", "Weather", "Games", "Settings",
    "Screensaver", "Files", "File Server",
    "Tic-Tac-Toe", "Minesweeper", "15 Puzzle", "Sudoku",
    "New game", "Back", "No SD card",
    "No SD card — Reader/HA unavailable", "SD card required",
    "You won!", "X wins", "AI wins", "O wins", "Draw", "Your move (X)",
    "Move: X", "Move: O", "vs AI", "2 players",
    "You win!", "Boom!", "Mines: %d   Mode: %s", "Flag", "Reveal",
    "Solved in %d moves, %02u:%02u", "Moves: %d",
    "Solved!", "Erase",
    "No /paperos/ha_dashboard.json", "or file is invalid", "Locked", "Unlocked",
    "No books in /paperos/books/", "p. %u",
    "offline", "No data — tap to refresh",
    "Open on your computer:", "Finder: Cmd+K -> this address", "Root: /paperos",
    "Back (G38) — stop", "Insert a card and reopen",
    "WiFi not configured", "Settings -> WiFi", "Connecting to WiFi…",
    "Connection failed", "Check Settings -> WiFi",
    "Failed to start server",
    "Loading…",
    "Reading", "Time", "About", "Password", "Hide password",
    "Show password", "Test", "Hide token", "Show token", "Check",
    "Font size", "Margins", "Sync NTP", "%u sec", "Idle to screensaver",
    "%u min", "Photo rotation", "Latitude", "Longitude", "Refresh interval",
    "Sensor t° offset", "Update now", "Version",
    "Sync config.json", "Reboot",
    "Connecting...", "Error %d", "Checking HA...", "WiFi: not connected",
    "HA available (HTTP %d)", "Auth: token rejected (401)", "Endpoint 404 — check URL",
    "Updating weather...", "OK: %+d\xC2\xB0, days %u", "Failed to fetch data",
    "Time updated", "NTP failed", "Synced", "JSON error",
    "Language",
    "Clear", "Partly cloudy", "Cloudy", "Fog", "Rain", "Snow", "Thunder", "—",
    "Updated %s",
};
static_assert(sizeof(kEn) / sizeof(*kEn) == static_cast<size_t>(Str::_Count),
              "kEn must have exactly Str::_Count entries");

extern const char* const kEnMon[12] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
extern const char* const kEnDow[7]  = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };

} // namespace paperos
