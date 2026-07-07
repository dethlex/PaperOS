#include "PngDraw.h"
#include <PNGdec.h>

namespace paperos {

PNG& sharedPng() {
    static PNG instance;   // single process-wide decoder (see PngDraw.h)
    return instance;
}

namespace {
// Single-threaded firmware: file-local decoder state, same pattern as the
// screensaver's PNG callbacks.
PNG& s_png = sharedPng();
std::vector<uint8_t>* s_gray = nullptr;
int s_w = 0, s_h = 0;

constexpr int kMaxLineW = 320;   // size of the stack line buffer in drawCb

int drawCb(PNGDRAW* pDraw) {
    if (!s_gray) return 0;
    if (pDraw->y < 0 || pDraw->y >= s_h) return 1;   // defensive: malformed rows are skipped
    uint16_t line[kMaxLineW];                      // thumbnails are <= 260 wide
    int w = pDraw->iWidth > kMaxLineW ? kMaxLineW : pDraw->iWidth;
    s_png.getLineAsRGB565(pDraw, line, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);
    uint8_t* row = s_gray->data() + static_cast<size_t>(pDraw->y) * s_w;
    for (int x = 0; x < w && x < s_w; ++x) {
        uint16_t c = line[x];
        uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
        row[x] = static_cast<uint8_t>(((r << 3) + (g << 2) + (b << 3)) / 3);
    }
    return 1;
}
} // namespace

bool pngDecodeToGray(const uint8_t* png, size_t len,
                     std::vector<uint8_t>& gray, int& w, int& h,
                     int maxW, int maxH) {
    if (maxW > kMaxLineW) maxW = kMaxLineW;   // internal line buffer bound
    int rc = s_png.openRAM(const_cast<uint8_t*>(png), static_cast<int>(len), drawCb);
    if (rc != PNG_SUCCESS) return false;
    w = s_png.getWidth();
    h = s_png.getHeight();
    if (w <= 0 || h <= 0 || w > maxW || h > maxH) { s_png.close(); return false; }
    gray.assign(static_cast<size_t>(w) * h, 255);
    s_gray = &gray;
    s_w = w;
    s_h = h;
    rc = s_png.decode(nullptr, 0);
    s_png.close();
    s_gray = nullptr;
    if (rc != PNG_SUCCESS) { gray.clear(); return false; }
    return true;
}

} // namespace paperos
