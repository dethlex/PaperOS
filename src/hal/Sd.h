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

    // --- WebDAV-примитивы ---
    bool exists(const char* path);
    // false, если пути нет. Иначе заполняет isDir/size/mtime.
    bool stat(const char* path, bool& isDir, uint32_t& size, uint32_t& mtime);
    // Содержимое папки (файлы И подпапки, без фильтра по расширению). Скрытые/AppleDouble пропускаются.
    std::vector<DirEntry> listDir(const char* path);
    bool makeDir(const char* path);      // MKCOL
    bool removePath(const char* path);   // файл или пустая папка
    bool removeTree(const char* path);   // рекурсивно (DELETE папки)
    bool renamePath(const char* from, const char* to);  // MOVE
    File openRead(const char* path);     // GET (FILE_READ)
    File openWrite(const char* path);    // PUT (FILE_WRITE, truncate)

private:
    bool ok_ = false;
};

} // namespace paperos
