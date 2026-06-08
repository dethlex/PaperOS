#pragma once
#include <vector>
#include <string>

namespace paperos {

// Returns the user-selectable app ids in launcher display order:
// drops the service ids "launcher" and "screensaver", keeps the rest in their
// original order, and pins "settings" last (if present). This is the single
// source of truth for "Настройки всегда последние".
std::vector<std::string> launcherAppOrder(const std::vector<std::string>& ids);

} // namespace paperos
