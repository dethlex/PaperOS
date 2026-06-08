#pragma once
#include <vector>
#include <string>

namespace paperos {

class Sd;

class Storage {
public:
    explicit Storage(Sd& sd) : sd_(sd) {}

    // Ensure that /paperos/{books,wallpapers,cache,logs,fonts} exist.
    bool ensureLayout();

    // Append to log file, rotating when exceeding kLogRotateBytes.
    // Path format: /paperos/logs/paperos.{0,1,2}.log
    void appendLog(const char* line);

    // Returns true if /paperos/config.json exists (used to trigger bootstrap import).
    bool hasBootstrapConfig();

    static constexpr size_t kLogRotateBytes = 256 * 1024;
    static constexpr uint8_t kLogFiles = 3;

private:
    Sd& sd_;
};

} // namespace paperos
