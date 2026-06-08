#include "WebDavPath.h"
#include <vector>

namespace paperos {

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int hi = hexVal(in[i + 1]);
            int lo = hexVal(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

std::string urlEncode(const std::string& in) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        bool keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
        if (keep) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

bool mapUrlToSdPath(const std::string& urlPath, std::string& outSdPath) {
    outSdPath.clear();   // on any false return the output stays empty
    std::string decoded = urlDecode(urlPath);
    if (decoded.find('\0') != std::string::npos) return false;

    std::vector<std::string> segs;
    size_t i = 0;
    while (i < decoded.size()) {
        while (i < decoded.size() && decoded[i] == '/') ++i;   // eat slashes
        size_t start = i;
        while (i < decoded.size() && decoded[i] != '/') ++i;
        if (i > start) {
            std::string seg = decoded.substr(start, i - start);
            if (seg == ".") continue;          // current dir — skip
            if (seg == "..") return false;      // traversal — strictly forbidden
            segs.push_back(seg);
        }
    }

    std::string path = "/paperos";
    for (const auto& s : segs) { path.push_back('/'); path += s; }
    outSdPath = path;
    return true;
}

} // namespace paperos
