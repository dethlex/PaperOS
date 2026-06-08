#include "Keyboard.h"
#include "hal/Display.h"
#include "framework/ui/Fonts.h"
#include "framework/ui/IconKit.h"
#include <Arduino.h>
#include <vector>

namespace paperos {

// Uniform key grid. One reference of 11 columns (the widest letter row) fixes a
// single unit width, so letter keys are identical across every row; each row is
// centred. Wide keys span whole units (SPACE = 5, ENT/SYM/lang-toggle = 2) so
// the bottom row still fills the width. drawKeys() and onTouch() share
// layoutRow() → draw geometry and hit-testing can never drift apart.
static constexpr int16_t kKbMargin = 6;
static constexpr int16_t kKbUnits  = 11;
static constexpr int16_t kKbUnitW  = (kScreenW - 2 * kKbMargin) / kKbUnits;  // 48
static constexpr int16_t kKbPad    = 3;   // gap inset between adjacent keys

struct KeyBox { int16_t x; int16_t w; std::string token; };

static int keyUnits(const std::string& k) {
    if (k == "SPACE") return 5;
    if (k == "ENT" || k == "SYM" || k == "RU" || k == "EN") return 2;
    return 1;   // letters / digits / symbols / SHF / BSP
}

static std::vector<KeyBox> layoutRow(const std::vector<std::string>& keys) {
    int units = 0;
    for (const auto& k : keys) units += keyUnits(k);
    int16_t x = static_cast<int16_t>((kScreenW - units * kKbUnitW) / 2);  // centre the row
    std::vector<KeyBox> out;
    out.reserve(keys.size());
    for (const auto& k : keys) {
        int16_t w = static_cast<int16_t>(keyUnits(k) * kKbUnitW);
        out.push_back({x, w, k});
        x = static_cast<int16_t>(x + w);
    }
    return out;
}

// Each row is space-separated keys; "BSP", "ENT", "SHF", "SYM", "RU", "EN", "SPACE" are special.
static const char* kRowsEn[] = {
    "q w e r t y u i o p",
    "a s d f g h j k l",
    "SHF z x c v b n m BSP",
    "SYM RU SPACE ENT"
};
static const char* kRowsRu[] = {
    "й ц у к е н г ш щ з х",
    "ф ы в а п р о л д ж э",
    "SHF я ч с м и т ь б ю BSP",
    "SYM EN SPACE ENT"
};
static const char* kRowsSym[] = {
    "1 2 3 4 5 6 7 8 9 0",
    "- _ + = ( ) [ ] { }",
    "@ # $ % & * / . : ; BSP",
    "EN RU SPACE ENT"
};

const char* const* Keyboard::currentRows(int& rc, int& cpr) const {
    (void)cpr;
    switch (layout_) {
        case KeyboardLayout::RU:      rc = 4; return kRowsRu;
        case KeyboardLayout::Symbols: rc = 4; return kRowsSym;
        default:                      rc = 4; return kRowsEn;
    }
}

void Keyboard::open(const std::string& initial, std::function<void(const std::string&)> on_done) {
    value_ = initial;
    on_done_ = std::move(on_done);
    is_open_ = true;
}

void Keyboard::drawValueField(M5EPD_Canvas& c) {
    Fonts fonts; fonts.apply(c, FontFace::Serif, 32);
    c.setTextColor(15);
    std::string shown = masked_ ? std::string(value_.size(), '*') : value_;
    const int16_t fy = kKbTop - 80;
    c.fillRect(20, fy, kScreenW - 40, 60, 0);
    // Bold focus border: 3 nested ink rects (was a single gray 1 px rect — gray
    // is also dropped by the A2/Quick push, so ink keeps it visible).
    c.drawRect(20, fy,     kScreenW - 40, 60, 15);
    c.drawRect(21, fy + 1, kScreenW - 42, 58, 15);
    c.drawRect(22, fy + 2, kScreenW - 44, 56, 15);
    c.setTextDatum(TL_DATUM);
    c.drawString(shown.c_str(), 32, fy + 14);
}

static std::vector<std::string> splitRow(const std::string& row) {
    std::vector<std::string> keys;
    size_t p = 0;
    while (p < row.size()) {
        size_t sp = row.find(' ', p);
        if (sp == std::string::npos) { keys.push_back(row.substr(p)); break; }
        keys.push_back(row.substr(p, sp - p));
        p = sp + 1;
    }
    return keys;
}

// Special tokens that are NOT letters — shift never changes them or their label.
static bool isSpecialToken(const std::string& k) {
    return k == "BSP" || k == "ENT" || k == "SHF" || k == "SYM" ||
           k == "RU"  || k == "EN"  || k == "SPACE";
}

// UTF-8 uppercase for letter keys. Handles ASCII a-z and Russian а-я + ё.
// Anything else (digits, punctuation, symbols) is passed through unchanged.
static std::string toUpperUtf8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        uint8_t b = static_cast<uint8_t>(s[i]);
        if (b < 0x80) {
            if (b >= 'a' && b <= 'z') b = static_cast<uint8_t>(b - 32);
            out.push_back(static_cast<char>(b));
            i++;
        } else if ((b & 0xE0) == 0xC0 && i + 1 < s.size()) {
            uint32_t cp = (static_cast<uint32_t>(b & 0x1F) << 6) |
                          (static_cast<uint8_t>(s[i + 1]) & 0x3F);
            uint32_t upper = cp;
            if      (cp >= 0x0430 && cp <= 0x044F) upper = cp - 0x20;  // а..я → А..Я
            else if (cp == 0x0451)                 upper = 0x0401;     // ё → Ё
            if (upper < 0x80) {
                out.push_back(static_cast<char>(upper));
            } else {
                out.push_back(static_cast<char>(0xC0 | (upper >> 6)));
                out.push_back(static_cast<char>(0x80 | (upper & 0x3F)));
            }
            i += 2;
        } else {
            out.push_back(s[i]);
            i++;
        }
    }
    return out;
}

void Keyboard::drawKeys(M5EPD_Canvas& c) {
    int rc; int cpr = 0;
    const char* const* rows = currentRows(rc, cpr);
    const int16_t y0 = kKbTop;
    const int16_t row_h = (kScreenH - kKbTop) / rc;
    // Clear the entire keys area before redraw — drawRect + drawString alone
    // do NOT erase what was there before, so layout switches (EN ↔ RU ↔ Sym),
    // shift toggles, and key presses would leave the previous glyphs and
    // borders bleeding through the new ones.
    c.fillRect(0, y0, kScreenW, kScreenH - y0, 0);
    Fonts fonts; fonts.apply(c, FontFace::Serif, 30);
    c.setTextColor(15);
    for (int r = 0; r < rc; ++r) {
        auto boxes = layoutRow(splitRow(rows[r]));
        int16_t y = static_cast<int16_t>(y0 + r * row_h);
        for (const auto& b : boxes) {
            int16_t kx = static_cast<int16_t>(b.x + kKbPad);
            int16_t ky = static_cast<int16_t>(y + kKbPad);
            int16_t kw = static_cast<int16_t>(b.w - 2 * kKbPad);
            int16_t kh = static_cast<int16_t>(row_h - 2 * kKbPad);
            int16_t cx = static_cast<int16_t>(kx + kw / 2);
            int16_t cy = static_cast<int16_t>(ky + kh / 2);
            // Outline in INK (15), not gray — gray (12) is dropped by the A2/Quick
            // push used on each keypress/layout-switch, which erased the borders.
            c.drawRect(kx, ky, kw, kh, 15);

            const std::string& key = b.token;
            if (key == "SHF") {
                int s = kh * 3 / 5;
                IconKit::shift(c, cx - s / 2, cy - s / 2, s);
                if (shift_) c.fillRect(kx + kw / 4, ky + kh - 9, kw / 2, 4, 15);  // armed indicator
            } else if (key == "BSP") {
                int s = kh * 3 / 5;
                IconKit::backspace(c, cx - s / 2, cy - s / 2, s);
            } else if (key == "SPACE") {
                c.fillRect(cx - kw / 4, cy + kh / 4, kw / 2, 4, 15);              // space-bar mark
            } else {
                // Letters show uppercase while shift is armed; labels centred via
                // MC_DATUM (font-metric centring — reliable for the single Cyrillic
                // glyphs where textWidth under-measures).
                std::string upper_buf;
                const char* lbl = key.c_str();
                if (shift_ && !isSpecialToken(key)) { upper_buf = toUpperUtf8(key); lbl = upper_buf.c_str(); }
                c.setTextDatum(MC_DATUM);
                c.drawString(lbl, cx, cy);
            }
        }
    }
    c.setTextDatum(TL_DATUM);   // restore default for subsequent draws
}

void Keyboard::render(M5EPD_Canvas& c) {
    if (!is_open_) return;
    drawValueField(c);
    drawKeys(c);
}

bool Keyboard::onTouch(int16_t x, int16_t y, Display& display) {
    if (!is_open_) return false;
    if (y < kKbTop) return false;

    int rc; int cpr = 0;
    const char* const* rows = currentRows(rc, cpr);
    const int16_t row_h = (kScreenH - kKbTop) / rc;
    int r = (y - kKbTop) / row_h;
    if (r < 0 || r >= rc) return true;

    // Hit-test against the SAME layout drawKeys() uses, so taps land exactly on
    // the drawn keys. A tap in the centring margin / inter-key gap is consumed
    // with no action.
    auto boxes = layoutRow(splitRow(rows[r]));
    const std::string* hit = nullptr;
    for (const auto& b : boxes) {
        if (x >= b.x && x < b.x + b.w) { hit = &b.token; break; }
    }
    if (!hit) return true;
    const std::string& k = *hit;

    if (k == "BSP") {
        // BSP removes one UTF-8 codepoint (1-4 bytes), not one byte — otherwise
        // a single Backspace on a Cyrillic char would leave a dangling
        // continuation byte and render as U+FFFD.
        while (!value_.empty()) {
            uint8_t last = static_cast<uint8_t>(value_.back());
            value_.pop_back();
            if ((last & 0xC0) != 0x80) break;  // hit the lead byte
        }
    }
    else if (k == "ENT")   { if (on_done_) on_done_(value_); }
    else if (k == "SHF")   { shift_ = !shift_; }
    else if (k == "SYM")   { layout_ = KeyboardLayout::Symbols; }
    else if (k == "RU")    { layout_ = KeyboardLayout::RU; }
    else if (k == "EN")    { layout_ = KeyboardLayout::EN; }
    else if (k == "SPACE") { value_ += ' '; /* spaces don't consume shift */ }
    else {
        // Regular character — emit as uppercase if shift is armed, then
        // release shift (single-shot semantics).
        value_ += shift_ ? toUpperUtf8(k) : k;
        if (shift_) shift_ = false;
    }

    // Re-render the keyboard region with fast A2 update.
    auto& c = display.canvas();
    drawValueField(c);
    drawKeys(c);
    display.pushRegion({0, kKbTop - 100, kScreenW, kScreenH - (kKbTop - 100)}, PushMode::Quick);
    return true;
}

} // namespace paperos
