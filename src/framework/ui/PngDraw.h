#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

class PNG;   // fwd-decl (PNGdec.h)

namespace paperos {

// Decode a PNG held in RAM into an 8-bit grayscale buffer (0=black..255=white,
// luminance, row-major w*h). Rejects images larger than maxW x maxH. Returns
// false on any decode error; on success `gray` is resized to w*h.
// maxW is additionally clamped to an internal 320-px line-buffer limit.
bool pngDecodeToGray(const uint8_t* png, size_t len,
                     std::vector<uint8_t>& gray, int& w, int& h,
                     int maxW, int maxH);

// Process-wide PNGdec decoder instance. PNGIMAGE embeds a large (~50 KB)
// static zlib/pixel/file buffer; the firmware is single-threaded with only
// one app in the foreground at a time, so every PNG decode site (screensaver
// wallpaper, printer thumbnail, ...) shares this one instance instead of
// each paying for its own static allocation (a second instance blew the
// ESP32 DRAM budget — see printer thumbnail task).
PNG& sharedPng();

} // namespace paperos
