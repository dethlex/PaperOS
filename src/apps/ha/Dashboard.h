#pragma once
#include <string>
#include <vector>

namespace paperos {

enum class WidgetKind { Toggle, Sensor, Action, Lock };

struct DashboardItem {
    std::string entity_id;
    std::string label;
    std::string unit;
    WidgetKind kind;
};

struct DashboardGroup {
    std::string title;
    std::vector<DashboardItem> items;
};

class Dashboard {
public:
    bool parse(const char* json);
    bool loadFromSd(const char* path);
    const std::vector<DashboardGroup>& groups() const { return groups_; }
private:
    std::vector<DashboardGroup> groups_;
};

} // namespace paperos
