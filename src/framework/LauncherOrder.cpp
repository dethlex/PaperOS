#include "LauncherOrder.h"

namespace paperos {

std::vector<std::string> launcherAppOrder(const std::vector<std::string>& ids) {
    std::vector<std::string> out;
    bool has_settings = false;
    for (const auto& id : ids) {
        if (id == "launcher" || id == "screensaver") continue;  // service apps
        if (id == "settings") { has_settings = true; continue; }
        out.push_back(id);
    }
    if (has_settings) out.push_back("settings");   // always last
    return out;
}

} // namespace paperos
