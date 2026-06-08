#pragma once
#include <SD.h>
#include <vector>
#include <string>

namespace paperos {

class Sd {
public:
    struct DirEntry { std::string name; bool isDir; uint32_t size; uint32_t mtime; };

    bool begin();                       // returns true on success
    bool present() const { return ok_; }
    // List files under `dir` matching extension (e.g. ".txt"). Recursive=false.
    std::vector<std::string> list(const char* dir, const char* ext);
    // Read full file into buffer. Returns bytes read or 0 on error.
    size_t readAll(const char* path, std::vector<uint8_t>& out, size_t maxBytes = 0);
    // Write file atomically: write `.tmp`, then rename.
    bool writeAtomic(const char* path, const uint8_t* data, size_t len);

    // --- WebDAV primitives ---
    bool exists(const char* path);
    // false if the path doesn't exist. Otherwise fills isDir/size/mtime.
    bool stat(const char* path, bool& isDir, uint32_t& size, uint32_t& mtime);
    // Directory contents (files AND subdirs, no extension filter). Hidden/AppleDouble skipped.
    std::vector<DirEntry> listDir(const char* path);
    bool makeDir(const char* path);      // MKCOL
    bool removePath(const char* path);   // file or empty directory
    bool removeTree(const char* path);   // recursive (DELETE of a directory)
    bool renamePath(const char* from, const char* to);  // MOVE
    File openRead(const char* path);     // GET (FILE_READ)
    File openWrite(const char* path);    // PUT (FILE_WRITE, truncate)

private:
    bool ok_ = false;
};

} // namespace paperos
