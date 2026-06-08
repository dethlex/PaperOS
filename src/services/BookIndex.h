#pragma once
#include <vector>
#include <string>
#include <stdint.h>
#include "apps/reader/EncodingDetect.h"

namespace paperos {

class Sd;

struct BookEntry {
    std::string path;            // /paperos/books/foo.txt
    std::string title;           // derived from filename
    size_t size_bytes = 0;
};

class BookIndex {
public:
    explicit BookIndex(Sd& sd) : sd_(sd) {}

    void begin();                                 // lazy — scan on first books()

    const std::vector<BookEntry>& books();        // triggers scan if not done

    // Detect encoding for the given book (reads first 4 KB).
    Encoding detectEncoding(const char* path);

    // Read a chunk: from `offset`, up to `maxBytes`. Returns actual bytes read.
    size_t readChunk(const char* path, uint32_t offset, uint8_t* out, size_t maxBytes);

private:
    Sd& sd_;
    std::vector<BookEntry> books_;
    bool scanned_ = false;
    void scan();
};

} // namespace paperos
