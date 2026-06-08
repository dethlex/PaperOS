#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace paperos {

struct DavEntry {
    std::string name;       // basename (for href and displayname)
    bool        isDir = false;
    uint32_t    size  = 0;  // bytes (for files)
    uint32_t    mtime = 0;  // unix; 0 → omit getlastmodified
};

// RFC1123 GMT date from unix time: "Thu, 01 Jan 1970 00:00:00 GMT".
std::string httpDate(uint32_t unixTime);

// 207 Multistatus. baseHref — href of the resource itself ("/", "/books/").
// Always emits a <response> for the resource itself; children=[] → Depth:0,
// otherwise plus one <response> per child. selfIsDir marks baseHref as a collection.
// selfSize/selfMtime — size and mtime of the resource ITSELF (for PROPFIND on a file:
// otherwise getcontentlength would always be 0). Ignored for a directory.
std::string buildPropfindXml(const std::string& baseHref, bool selfIsDir,
                             const std::vector<DavEntry>& children,
                             uint32_t selfSize = 0, uint32_t selfMtime = 0);

// LOCK response with an opaque token (fake lock for Finder).
std::string buildLockXml(const std::string& token, int timeoutSec);

// PROPPATCH response: 207 Multistatus, all properties "set successfully" (200 OK).
// We don't actually store properties — this is needed for macOS Finder compatibility,
// which sends PROPPATCH (modification date etc.) right after PUT and, without a 207,
// fails the copy with error -36.
std::string buildProppatchXml(const std::string& href);

} // namespace paperos
