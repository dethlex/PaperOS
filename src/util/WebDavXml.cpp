#include "WebDavXml.h"
#include "WebDavPath.h"   // urlEncode
#include <cstdio>
#include <ctime>

namespace paperos {

std::string httpDate(uint32_t unixTime) {
    static const char* kWd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* kMo[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                "Jul","Aug","Sep","Oct","Nov","Dec"};
    time_t t = static_cast<time_t>(unixTime);
    struct tm g;
    gmtime_r(&t, &g);
    char buf[40];
    snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT",
             kWd[g.tm_wday], g.tm_mday, kMo[g.tm_mon], g.tm_year + 1900,
             g.tm_hour, g.tm_min, g.tm_sec);
    return std::string(buf);
}

static void appendResponse(std::string& xml, const std::string& href,
                           bool isDir, uint32_t size, uint32_t mtime) {
    xml += "<D:response><D:href>";
    xml += urlEncode(href);
    xml += "</D:href><D:propstat><D:prop><D:resourcetype>";
    if (isDir) xml += "<D:collection/>";
    xml += "</D:resourcetype>";
    if (!isDir) {
        xml += "<D:getcontentlength>";
        xml += std::to_string(size);
        xml += "</D:getcontentlength>";
    }
    if (mtime > 0) {
        xml += "<D:getlastmodified>";
        xml += httpDate(mtime);
        xml += "</D:getlastmodified>";
    }
    xml += "</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response>";
}

std::string buildPropfindXml(const std::string& baseHref, bool selfIsDir,
                             const std::vector<DavEntry>& children,
                             uint32_t selfSize, uint32_t selfMtime) {
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    xml += "<D:multistatus xmlns:D=\"DAV:\">";
    appendResponse(xml, baseHref, selfIsDir, selfSize, selfMtime);

    std::string base = baseHref;
    if (base.empty() || base.back() != '/') base.push_back('/');
    for (const auto& e : children) {
        std::string href = base + e.name;
        if (e.isDir) href.push_back('/');
        appendResponse(xml, href, e.isDir, e.size, e.mtime);
    }
    xml += "</D:multistatus>";
    return xml;
}

std::string buildLockXml(const std::string& token, int timeoutSec) {
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    xml += "<D:prop xmlns:D=\"DAV:\"><D:lockdiscovery><D:activelock>";
    xml += "<D:locktype><D:write/></D:locktype>";
    xml += "<D:lockscope><D:exclusive/></D:lockscope>";
    xml += "<D:depth>infinity</D:depth>";
    xml += "<D:timeout>Second-";
    xml += std::to_string(timeoutSec);
    xml += "</D:timeout><D:locktoken><D:href>";
    xml += token;
    xml += "</D:href></D:locktoken></D:activelock></D:lockdiscovery></D:prop>";
    return xml;
}

std::string buildProppatchXml(const std::string& href) {
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    xml += "<D:multistatus xmlns:D=\"DAV:\"><D:response><D:href>";
    xml += urlEncode(href);
    xml += "</D:href><D:propstat><D:prop/><D:status>HTTP/1.1 200 OK</D:status></D:propstat>";
    xml += "</D:response></D:multistatus>";
    return xml;
}

} // namespace paperos
