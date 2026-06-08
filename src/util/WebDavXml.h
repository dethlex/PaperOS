#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace paperos {

struct DavEntry {
    std::string name;       // basename (для href и displayname)
    bool        isDir = false;
    uint32_t    size  = 0;  // байты (для файлов)
    uint32_t    mtime = 0;  // unix; 0 → getlastmodified опускаем
};

// RFC1123 GMT-дата из unix-времени: "Thu, 01 Jan 1970 00:00:00 GMT".
std::string httpDate(uint32_t unixTime);

// 207 Multistatus. baseHref — href самого ресурса ("/", "/books/").
// Всегда эмитит <response> для самого ресурса; children=[] → Depth:0,
// иначе + по <response> на каждого ребёнка. selfIsDir помечает baseHref коллекцией.
// selfSize/selfMtime — размер и mtime САМОГО ресурса (для PROPFIND на файл:
// иначе getcontentlength был бы всегда 0). Для папки size игнорируется.
std::string buildPropfindXml(const std::string& baseHref, bool selfIsDir,
                             const std::vector<DavEntry>& children,
                             uint32_t selfSize = 0, uint32_t selfMtime = 0);

// Ответ на LOCK с непрозрачным токеном (фейковый лок для Finder).
std::string buildLockXml(const std::string& token, int timeoutSec);

// Ответ на PROPPATCH: 207 Multistatus, все свойства «успешно установлены» (200 OK).
// Свойства фактически не храним — это требуется для совместимости с macOS Finder,
// который шлёт PROPPATCH (дата модификации и пр.) сразу после PUT и без 207 валит
// копирование ошибкой -36.
std::string buildProppatchXml(const std::string& href);

} // namespace paperos
