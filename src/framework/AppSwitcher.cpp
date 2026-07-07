#include "AppSwitcher.h"
#include "util/Logger.h"

namespace paperos {

void AppSwitcher::registerApp(const std::string& id, Factory f) {
    factories_[id] = std::move(f);
    order_.push_back(id);
}

void AppSwitcher::switchTo(const std::string& id) {
    auto it = factories_.find(id);
    if (it == factories_.end()) { LOG_WARN("switcher", "unknown app %s", id.c_str()); return; }
    if (current_ && ctx_) current_->leave(*ctx_);
    current_.reset(it->second());
    current_id_ = id;
    if (ctx_) current_->enter(*ctx_);
    LOG_INFO("switcher", "switched to %s", id.c_str());
}

uint8_t AppSwitcher::indexOf(const std::string& id) const {
    for (size_t i = 0; i < order_.size(); ++i) if (order_[i] == id) return static_cast<uint8_t>(i);
    return 0;
}

std::string AppSwitcher::idOfIndex(uint8_t i) const {
    return i < order_.size() ? order_[i] : (order_.empty() ? std::string{} : order_[0]);
}

std::string AppSwitcher::titleOf(const std::string& id) const {
    auto it = factories_.find(id);
    if (it == factories_.end()) return id;       // unknown id: fall back to the id itself
    std::unique_ptr<IApp> tmp(it->second());     // transient instance — ctors must stay cheap
    return tmp->title();                          // tr() -> static lang table; copied into std::string
}

} // namespace paperos
