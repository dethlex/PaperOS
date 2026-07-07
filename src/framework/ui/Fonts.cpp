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
    // Font state in M5EPD is PROCESS-GLOBAL: TFT_eSPI declares _font_face,
    // _is_freetype_loaded and _render_map as static members shared by every
    // canvas (In_eSPI.h). loadFont() destroys ALL cached glyph renders and
    // re-parses the TTF, so we track the one global face and reload only on
    // a real change; createRender() is skipped when the size is already in
    // the shared render map. Only Fonts::apply may call loadFont — a direct
    // canvas.loadFont() elsewhere would silently desync this tracker.
    static bool     s_have_face = false;
    static FontFace s_face      = FontFace::Serif;
    if (!s_have_face || s_face != face) {
        if (face == FontFace::Serif) {
            c.loadFont(kSerifStart, kSerifEnd - kSerifStart);
        } else {
            c.loadFont(kMonoStart, kMonoEnd - kMonoStart);
        }
        s_face      = face;
        s_have_face = true;
    }
    // On a face switch loadFont wiped the shared renders → isRenderExist is
    // false and we recreate; on the warm path the glyph cache is reused.
    if (!c.isRenderExist(static_cast<uint16_t>(pixel_size))) {
        c.createRender(pixel_size, 256);
    }
    c.setTextSize(pixel_size);
}

} // namespace paperos
