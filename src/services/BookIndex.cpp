#include "BookIndex.h"
#include "hal/Sd.h"
#include <SD.h>
#include <algorithm>

namespace paperos {

void BookIndex::begin() { /* lazy: scan happens on first books() */ }

const std::vector<BookEntry>& BookIndex::books() {
    if (!scanned_) scan();
    return books_;
}

void BookIndex::scan() {
    books_.clear();
    if (!sd_.present()) { scanned_ = true; return; }
    auto paths = sd_.list("/paperos/books", ".txt");
    for (const auto& p : paths) {
        BookEntry e;
        e.path = p;
        // derive title from filename without extension
        auto slash = p.find_last_of('/');
        std::string name = slash == std::string::npos ? p : p.substr(slash + 1);
        if (name.size() > 4) name = name.substr(0, name.size() - 4);
        e.title = name;
        File f = SD.open(p.c_str(), FILE_READ);
        if (f) { e.size_bytes = f.size(); f.close(); }
        books_.push_back(std::move(e));
    }
    std::sort(books_.begin(), books_.end(),
              [](const BookEntry& a, const BookEntry& b){ return a.title < b.title; });
    scanned_ = true;
}

Encoding BookIndex::detectEncoding(const char* path) {
    if (!sd_.present()) return Encoding::Utf8;
    File f = SD.open(path, FILE_READ);
    if (!f) return Encoding::Utf8;
    uint8_t buf[4096];
    size_t n = f.read(buf, sizeof(buf));
    f.close();
    return paperos::detectEncoding(buf, n);
}

size_t BookIndex::readChunk(const char* path, uint32_t offset, uint8_t* out, size_t maxBytes) {
    if (!sd_.present()) return 0;
    File f = SD.open(path, FILE_READ);
    if (!f) return 0;
    if (!f.seek(offset)) { f.close(); return 0; }
    size_t got = f.read(out, maxBytes);
    f.close();
    return got;
}

} // namespace paperos
