#pragma once
#include <string>

namespace paperos {

// Percent-decode ("%20"→" "). Truncated/malformed "%" stays literal.
std::string urlDecode(const std::string& in);

// Percent-encode for hrefs in XML. Preserves unreserved + '/' (path separator).
std::string urlEncode(const std::string& in);

// Request URL path → absolute path under the /paperos root.
// false if, after normalization, the path escapes the root: any ".." segment,
// or a null byte in the decoded string. On false, outSdPath is cleared.
// "/" -> "/paperos"; "/books/x.txt" -> "/paperos/books/x.txt".
// NB: percent-decode runs BEFORE splitting into segments, so a "%2F" inside a
// segment is treated as a path separator (not a literal slash). On FAT32 a
// filename can't contain '/', so this is harmless.
bool mapUrlToSdPath(const std::string& urlPath, std::string& outSdPath);

} // namespace paperos
