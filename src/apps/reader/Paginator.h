#pragma once
#include <stdint.h>
#include <vector>
#include "apps/reader/EncodingDetect.h"

namespace paperos {

class FontMetrics {
public:
    virtual ~FontMetrics() = default;
    virtual int advanceFor(uint32_t codepoint) const = 0;
    virtual int lineHeight() const = 0;
};

struct PageBreakerConfig {
    int page_w_px;
    int page_h_px;
    int margin_px;
    const FontMetrics* metrics;
    Encoding encoding;
};

class PageBreaker {
public:
    explicit PageBreaker(PageBreakerConfig cfg) : cfg_(cfg) {}

    // Returns byte-offsets of each page start. pages[0] is always 0.
    std::vector<uint32_t> paginate(const uint8_t* data, size_t len);

private:
    PageBreakerConfig cfg_;
};

} // namespace paperos
