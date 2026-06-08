#pragma once
#include "App.h"
#include <map>
#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace paperos {

class AppSwitcher {
public:
    using Factory = std::function<IApp*()>;

    void registerApp(const std::string& id, Factory factory);

    // Switch to app by id. Existing app gets leave(); new app is constructed and enter()ed.
    void switchTo(const std::string& id);

    // Request from inside an app's onInput/tick — actual switch happens at end of loop tick.
    void requestSwitch(const std::string& id) { pending_ = id; }
    bool hasPending() const { return !pending_.empty(); }
    void applyPending() { if (!pending_.empty()) { auto p = pending_; pending_.clear(); switchTo(p); } }

    IApp* current() { return current_.get(); }
    const std::vector<std::string>& registeredIds() const { return order_; }
    uint8_t indexOf(const std::string& id) const;
    std::string idOfIndex(uint8_t index) const;

    void setContext(AppContext* ctx) { ctx_ = ctx; }

private:
    std::map<std::string, Factory> factories_;
    std::vector<std::string> order_;          // registration order — stable indices for RTC slow mem
    std::unique_ptr<IApp> current_;
    std::string current_id_;
    std::string pending_;
    AppContext* ctx_ = nullptr;
};

} // namespace paperos
