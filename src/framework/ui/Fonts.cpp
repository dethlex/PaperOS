#include "Fonts.h"
#include "util/Logger.h"

extern const uint8_t kSerifStart[] asm("_binary_data_fonts_DejaVuSerif_ttf_start");
extern const uint8_t kSerifEnd[]   asm("_binary_data_fonts_DejaVuSerif_ttf_end");
extern const uint8_t kMonoStart[]  asm("_binary_data_fonts_DejaVuSansMono_Bold_ttf_start");
extern const uint8_t kMonoEnd[]    asm("_binary_data_fonts_DejaVuSansMono_Bold_ttf_end");

namespace paperos {

void Fonts::begin() {
    LOG_INFO("fonts", "fonts ready");
}

void Fonts::apply(M5EPD_Canvas& c, FontFace face, int pixel_size) {
    if (face == FontFace::Serif) {
        c.loadFont(kSerifStart, kSerifEnd - kSerifStart);
    } else {
        c.loadFont(kMonoStart, kMonoEnd - kMonoStart);
    }
    c.createRender(pixel_size, 256);
    c.setTextSize(pixel_size);
}

} // namespace paperos
