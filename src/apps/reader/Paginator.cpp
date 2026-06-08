#include "Paginator.h"
#include "util/Utf8.h"

namespace paperos {

static uint32_t nextCodepoint(const uint8_t* d, size_t len, size_t* pos, Encoding enc) {
    if (*pos >= len) return 0;
    if (enc == Encoding::Utf8) return utf8Next(d, len, pos);
    uint8_t b = d[*pos]; (*pos)++;
    return cp1251ToCodepoint(b);
}

std::vector<uint32_t> PageBreaker::paginate(const uint8_t* data, size_t len) {
    std::vector<uint32_t> pages;
    pages.push_back(0);
    if (len == 0) return pages;

    const int content_w = cfg_.page_w_px - 2 * cfg_.margin_px;
    const int content_h = cfg_.page_h_px - 2 * cfg_.margin_px;
    const int line_h = cfg_.metrics->lineHeight();

    int x = 0, y = 0;
    size_t pos = 0;
    size_t line_word_start = 0;
    int line_word_x = 0;

    while (pos < len) {
        size_t cp_start = pos;
        uint32_t cp = nextCodepoint(data, len, &pos, cfg_.encoding);
        if (cp == 0) break;

        if (cp == '\n') {
            x = 0;
            y += line_h;
            line_word_start = pos;
            line_word_x = 0;
        } else if (cp == ' ') {
            x += cfg_.metrics->advanceFor(cp);
            if (x > content_w) {
                x = 0; y += line_h;
            }
            line_word_start = pos;
            line_word_x = x;
        } else {
            int adv = cfg_.metrics->advanceFor(cp);
            if (x + adv > content_w) {
                x = (pos - line_word_start <= 32) ? line_word_x : 0;
                if (line_word_start > 0 && (pos - line_word_start <= 32)) {
                    pos = line_word_start;
                    cp_start = pos;
                }
                y += line_h;
                x = 0;
                continue;
            }
            x += adv;
        }

        if (y + line_h > content_h) {
            pages.push_back(static_cast<uint32_t>(cp_start));
            x = 0; y = 0;
            line_word_start = cp_start;
            line_word_x = 0;
        }
    }
    return pages;
}

} // namespace paperos
