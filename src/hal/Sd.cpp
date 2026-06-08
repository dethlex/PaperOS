#include "Sd.h"
#include <SPI.h>
#include <utility>
#include "util/Logger.h"

namespace paperos {

bool Sd::begin() {
    // M5.begin() already initialized the SPI bus and attempted to mount the
    // SD card. Just inspect the result instead of re-running SD.begin(), which
    // would emit a second [E][sd_diskio] f_mount failure for an absent card.
    ok_ = (SD.cardType() != CARD_NONE);
    if (!ok_) LOG_INFO("sd", "no card present");
    return ok_;
}

std::vector<std::string> Sd::list(const char* dir, const char* ext) {
    std::vector<std::string> result;
    if (!ok_) return result;
    File d = SD.open(dir);
    if (!d || !d.isDirectory()) return result;
    File f;
    while ((f = d.openNextFile())) {
        if (!f.isDirectory()) {
            // f.name() returns basename on arduino-esp32 >=2.x (pinned SDK).
            // Case-insensitive extension match so IMG_1234.JPG matches ".jpg".
            const char* name = f.name();
            // Skip hidden / AppleDouble metadata files (macOS adds these on
            // non-HFS volumes: "._foo.png" is a metadata sidecar, not a PNG).
            if (name[0] == '.') { f.close(); continue; }
            size_t nl = strlen(name), el = strlen(ext);
            if (nl >= el && strcasecmp(name + nl - el, ext) == 0) {
                std::string p = std::string(dir);
                if (p.back() != '/') p += '/';
                p += name;
                result.push_back(std::move(p));
            }
        }
        f.close();
    }
    return result;
}

size_t Sd::readAll(const char* path, std::vector<uint8_t>& out, size_t maxBytes) {
    if (!ok_) return 0;
    File f = SD.open(path, FILE_READ);
    if (!f) return 0;
    size_t total = f.size();
    if (maxBytes && total > maxBytes) total = maxBytes;
    out.resize(total);
    size_t got = f.read(out.data(), total);
    f.close();
    return got;
}

bool Sd::writeAtomic(const char* path, const uint8_t* data, size_t len) {
    if (!ok_) return false;
    std::string tmp = std::string(path) + ".tmp";
    File f = SD.open(tmp.c_str(), FILE_WRITE);
    if (!f) return false;
    size_t w = f.write(data, len);
    f.close();
    if (w != len) { SD.remove(tmp.c_str()); return false; }
    SD.remove(path);
    if (!SD.rename(tmp.c_str(), path)) {
        SD.remove(tmp.c_str());
        return false;
    }
    return true;
}

bool Sd::exists(const char* path) {
    if (!ok_) return false;
    return SD.exists(path);
}

bool Sd::stat(const char* path, bool& isDir, uint32_t& size, uint32_t& mtime) {
    if (!ok_) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    isDir = f.isDirectory();
    size  = isDir ? 0 : static_cast<uint32_t>(f.size());
    mtime = static_cast<uint32_t>(f.getLastWrite());   // 0 if the filesystem doesn't store it
    f.close();
    return true;
}

std::vector<Sd::DirEntry> Sd::listDir(const char* path) {
    std::vector<DirEntry> out;
    if (!ok_) return out;
    File d = SD.open(path);
    if (!d || !d.isDirectory()) { if (d) d.close(); return out; }
    File f;
    while ((f = d.openNextFile())) {
        const char* name = f.name();      // basename on pinned SDK
        if (name[0] != '.') {             // skip hidden / AppleDouble files
            DirEntry e;
            e.name  = name;
            e.isDir = f.isDirectory();
            e.size  = e.isDir ? 0 : static_cast<uint32_t>(f.size());
            e.mtime = static_cast<uint32_t>(f.getLastWrite());
            out.push_back(std::move(e));
        }
        f.close();
    }
    d.close();
    return out;
}

bool Sd::makeDir(const char* path)  { return ok_ && SD.mkdir(path); }

bool Sd::removePath(const char* path) {
    if (!ok_) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    bool isDir = f.isDirectory();
    f.close();
    return isDir ? SD.rmdir(path) : SD.remove(path);
}

bool Sd::removeTree(const char* path) {
    if (!ok_) return false;
    File d = SD.open(path, FILE_READ);
    if (!d) return false;
    if (!d.isDirectory()) { d.close(); return SD.remove(path); }
    // Snapshot children while the handle is open, THEN close d before recursing:
    // the FAT driver caps concurrently open descriptors (~10), and keeping d open
    // during recursion would exhaust them on a deep tree (then openNextFile
    // silently returns empty File handles and the listing is cut short).
    std::vector<std::pair<std::string, bool>> kids;
    File f;
    while ((f = d.openNextFile())) {
        std::string child = std::string(path);
        if (child.back() != '/') child += '/';
        child += f.name();
        kids.emplace_back(std::move(child), f.isDirectory());
        f.close();
    }
    d.close();
    bool ok = true;
    for (const auto& kid : kids) {
        ok = (kid.second ? removeTree(kid.first.c_str()) : SD.remove(kid.first.c_str())) && ok;
    }
    return SD.rmdir(path) && ok;
}

bool Sd::renamePath(const char* from, const char* to) {
    return ok_ && SD.rename(from, to);
}

File Sd::openRead(const char* path)  { return ok_ ? SD.open(path, FILE_READ)  : File(); }
File Sd::openWrite(const char* path) { return ok_ ? SD.open(path, FILE_WRITE) : File(); }

} // namespace paperos
