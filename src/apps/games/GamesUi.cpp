#include "apps/games/GamesUi.h"
#include "framework/ui/Fonts.h"

namespace paperos {
namespace gamesui {

void button(M5EPD_Canvas& c, Rect r, const char* label, bool active) {
    c.drawRect(r.x, r.y, r.w, r.h, 15);
    if (active) c.drawRect(r.x + 3, r.y + 3, r.w - 6, r.h - 6, 15);
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, 30);
    c.setTextColor(15);
    c.drawString(label, r.x + 16, r.y + (r.h - 30) / 2);
}

void centerAscii(M5EPD_Canvas& c, Rect r, const char* s, int px) {
    Fonts fonts;
    fonts.apply(c, FontFace::Serif, px);
    c.setTextColor(15);
    // MC_DATUM (centre-centre): the M5EPD FreeType drawString path centers on both
    // axes using the font's own glyph metrics (advance sum + pixel_size), so 1- and
    // multi-digit strings sit truly centered — no reliance on textWidth (which the
    // smooth font underestimates, esp. for capital Cyrillic — see CLAUDE.md §TTF).
    c.setTextDatum(MC_DATUM);
    c.drawString(s, r.x + r.w / 2, r.y + r.h / 2);
    c.setTextDatum(TL_DATUM);   // restore default for all other (left-aligned) text
}

} // namespace gamesui
} // namespace paperos
