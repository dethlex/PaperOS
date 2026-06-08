#pragma once
#include <M5EPD.h>

namespace paperos {

enum class FontFace { Serif, MonoBold };

class Fonts {
public:
    void begin();

    // Apply font face + pixel size to the given canvas.
    // Subsequent canvas.drawString uses this configuration.
    void apply(M5EPD_Canvas& c, FontFace face, int pixel_size);
};

} // namespace paperos
