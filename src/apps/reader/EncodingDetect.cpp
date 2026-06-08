#include "EncodingDetect.h"

namespace paperos {

static int utf8SeqLen(uint8_t b) {
    if ((b & 0x80) == 0)    return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 0;
}

Encoding detectEncoding(const uint8_t* data, size_t len) {
    if (len == 0 || !data) return Encoding::Utf8;
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        return Encoding::Utf8;

    size_t valid_seq = 0, high_total = 0;
    size_t i = 0;
    while (i < len) {
        uint8_t b = data[i];
        if (b >= 0x80) {
            high_total++;
            int sl = utf8SeqLen(b);
            if (sl >= 2 && i + sl <= len) {
                bool ok = true;
                for (int k = 1; k < sl; ++k)
                    if ((data[i + k] & 0xC0) != 0x80) { ok = false; break; }
                if (ok) { valid_seq += sl; i += sl; continue; }
            }
        }
        i++;
    }

    if (high_total == 0) return Encoding::Utf8;
    double ratio = static_cast<double>(valid_seq) / static_cast<double>(high_total);
    if (ratio > 0.85) return Encoding::Utf8;
    return Encoding::Cp1251;
}

} // namespace paperos
