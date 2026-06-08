#include "IconKit.h"
#include <cmath>

namespace paperos {

static constexpr uint16_t INK = 15;

// ---- shared helper (migrated from WeatherIcon.cpp) ----
static void cloud(M5EPD_Canvas& c, int x, int y, int s) {
    int by = y + s * 3 / 5;          // cloud baseline
    int r  = s / 6;
    c.fillCircle(x + s * 2 / 6, by, r, INK);
    c.fillCircle(x + s * 3 / 6, by - r, r + s / 12, INK);
    c.fillCircle(x + s * 4 / 6, by, r, INK);
    c.fillRect(x + s * 2 / 6, by, s * 2 / 6, r, INK);
}

// ---- weather (migrated 1:1 from WeatherIcon::draw) ----
void IconKit::weather(M5EPD_Canvas& c, WeatherCat cat, int x, int y, int s) {
    switch (cat) {
        case WeatherCat::Clear: {
            int cx = x + s / 2, cy = y + s / 2, r = s / 4;
            c.fillCircle(cx, cy, r, INK);
            for (int a = 0; a < 360; a += 45) {
                float rad = a * 3.14159265f / 180.0f;
                int x1 = cx + (int)((r + 3)     * cosf(rad));
                int y1 = cy + (int)((r + 3)     * sinf(rad));
                int x2 = cx + (int)((r + s / 6) * cosf(rad));
                int y2 = cy + (int)((r + s / 6) * sinf(rad));
                c.drawLine(x1, y1, x2, y2, INK);
            }
            break;
        }
        case WeatherCat::PartlyCloudy: {
            int cx = x + s * 2 / 5, cy = y + s * 2 / 5, r = s / 5;
            c.fillCircle(cx, cy, r, INK);
            cloud(c, x, y, s);
            break;
        }
        case WeatherCat::Cloudy:
            cloud(c, x, y, s);
            break;
        case WeatherCat::Fog:
            cloud(c, x, y, s * 3 / 4);
            for (int i = 0; i < 3; ++i)
                c.drawLine(x + s / 6, y + s * 3 / 4 + i * 6,
                           x + s * 5 / 6, y + s * 3 / 4 + i * 6, INK);
            break;
        case WeatherCat::Rain:
            cloud(c, x, y, s * 3 / 4);
            for (int i = 0; i < 3; ++i)
                c.drawLine(x + s / 3 + i * s / 6, y + s * 3 / 4,
                           x + s / 3 + i * s / 6 - 4, y + s * 3 / 4 + s / 6, INK);
            break;
        case WeatherCat::Snow:
            cloud(c, x, y, s * 3 / 4);
            for (int i = 0; i < 3; ++i)
                c.fillCircle(x + s / 3 + i * s / 6, y + s * 3 / 4 + s / 12, 2, INK);
            break;
        case WeatherCat::Thunder:
            cloud(c, x, y, s * 3 / 4);
            c.fillTriangle(x + s / 2,     y + s * 3 / 4,
                           x + s * 2 / 5, y + s * 3 / 4 + s / 6,
                           x + s / 2,     y + s * 3 / 4 + s / 6, INK);
            break;
        default:
            c.drawRect(x, y, s, s, INK);
            break;
    }
}

// ---- house (migrated 1:1 from WeatherIcon::drawHouse) ----
void IconKit::house(M5EPD_Canvas& c, int x, int y, int s) {
    int bx = x + s / 6, by = y + s * 2 / 5;
    int bw = s * 2 / 3, bh = s * 3 / 5 - s / 8;
    c.fillTriangle(x, by, x + s / 2, y + s / 10, x + s, by, INK);        // roof
    c.drawRect(bx, by, bw, bh, INK);                                     // body
    c.fillRect(x + s / 2 - s / 14, by + bh - s / 4, s / 7, s / 4, INK);  // door
}

// ---- drop (migrated 1:1 from WeatherIcon::drawDrop) ----
void IconKit::drop(M5EPD_Canvas& c, int x, int y, int s) {
    int cx = x + s / 2, r = s / 3;
    int cy = y + s - r - 1;
    c.fillCircle(cx, cy, r, INK);                                        // round bottom
    c.fillTriangle(cx - r, cy - r / 2, cx + r, cy - r / 2, cx, y, INK);  // pointed top
}

// ---- battery (migrated 1:1 from BatteryIcon::draw) ----
void IconKit::battery(M5EPD_Canvas& c, int x, int y, int w, int h, uint8_t percent) {
    if (percent > 100) percent = 100;
    if (w < 8 || h < 4) return;   // вырожденный размер — рисовать нечего

    const int nub_w  = 3;
    const int nub_h  = h / 3;
    const int body_w = w - nub_w;

    c.drawRect(x, y, body_w, h, INK);                                  // корпус
    c.fillRect(x + body_w, y + (h - nub_h) / 2, nub_w, nub_h, INK);    // носик

    const int inset = 2;
    const int avail = body_w - 2 * inset;
    const int fill  = (avail * percent) / 100;
    if (fill > 0) c.fillRect(x + inset, y + inset, fill, h - 2 * inset, INK);
}

// ---- keyboard key glyphs ----
void IconKit::shift(M5EPD_Canvas& c, int x, int y, int s) {
    int cx = x + s / 2;
    c.fillTriangle(cx, y + s / 6, x + s / 6, y + s / 2, x + s * 5 / 6, y + s / 2, INK);  // arrowhead
    c.fillRect(cx - s / 8, y + s / 2, s / 4, s / 3, INK);                                // stem
}

void IconKit::backspace(M5EPD_Canvas& c, int x, int y, int s) {
    int top = y + s / 4, bot = y + s * 3 / 4;
    int tipx = x + s / 8, bodyx = x + s * 3 / 8, rightx = x + s * 7 / 8;
    c.fillTriangle(tipx, y + s / 2, bodyx, top, bodyx, bot, INK);   // pointed left tip
    c.fillRect(bodyx, top, rightx - bodyx, bot - top, INK);         // body
    // White "x" punched into the ink body (0 = paper; renders in A2/Quick too).
    int ix0 = bodyx + (rightx - bodyx) / 4, ix1 = rightx - (rightx - bodyx) / 5;
    int iy0 = top + (bot - top) / 4, iy1 = bot - (bot - top) / 4;
    c.drawLine(ix0, iy0, ix1, iy1, 0);
    c.drawLine(ix1, iy0, ix0, iy1, 0);
}

// ---- new app glyphs (internal) ----
static void glyphBook(M5EPD_Canvas& c, int x, int y, int s) {
    int bw = s * 3 / 5, bh = s * 4 / 5;
    int bx = x + (s - bw) / 2, by = y + (s - bh) / 2;
    c.drawRect(bx, by, bw, bh, INK);                          // cover
    c.drawLine(bx + bw / 4, by, bx + bw / 4, by + bh, INK);   // spine
    for (int i = 1; i <= 3; ++i) {                            // page lines (proportional insets → valid at any size)
        int ly = by + bh * i / 4;
        c.drawLine(bx + bw / 4 + bw / 10, ly, bx + bw - bw / 8, ly, INK);
    }
}

static void glyphGear(M5EPD_Canvas& c, int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2;
    int r = s / 3;
    for (int a = 0; a < 360; a += 45) {                       // 8 teeth
        float rad = a * 3.14159265f / 180.0f;
        int tx = cx + (int)((r + s / 12) * cosf(rad));
        int ty = cy + (int)((r + s / 12) * sinf(rad));
        c.fillCircle(tx, ty, s / 12, INK);
    }
    c.fillCircle(cx, cy, r, INK);                             // body
    c.fillCircle(cx, cy, r / 2, 0);                           // hollow center — white punch-out, assumes white background
}

static void glyphFolder(M5EPD_Canvas& c, int x, int y, int s) {
    int fw = s * 4 / 5, fh = s * 3 / 5;
    int fx = x + (s - fw) / 2, fy = y + (s - fh) / 2 + s / 12;
    int tab_w = fw * 2 / 5, tab_h = s / 8;
    c.fillRect(fx, fy - tab_h, tab_w, tab_h, INK);            // tab (top-left)
    c.drawRect(fx, fy, fw, fh, INK);                          // body
}

static void glyphCloudSun(M5EPD_Canvas& c, int x, int y, int s) {
    int cx = x + s * 2 / 5, cy = y + s * 2 / 5, r = s / 6;
    c.fillCircle(cx, cy, r, INK);                             // sun
    for (int a = 0; a < 360; a += 45) {                       // rays
        float rad = a * 3.14159265f / 180.0f;
        int x1 = cx + (int)((r + 2)      * cosf(rad));
        int y1 = cy + (int)((r + 2)      * sinf(rad));
        int x2 = cx + (int)((r + s / 10) * cosf(rad));
        int y2 = cy + (int)((r + s / 10) * sinf(rad));
        c.drawLine(x1, y1, x2, y2, INK);
    }
    cloud(c, x + s / 6, y + s / 6, s * 5 / 6);                // cloud (lower-right)
}

static void glyphTile(M5EPD_Canvas& c, int x, int y, int s) {
    int m = s / 5;
    c.drawRect(x + m, y + m, s - 2 * m, s - 2 * m, INK);
    c.fillCircle(x + s / 2, y + s / 2, s / 12, INK);
}

static void glyphDice(M5EPD_Canvas& c, int x, int y, int s) {
    int m = s / 8;
    c.drawRect(x + m, y + m, s - 2 * m, s - 2 * m, INK);   // die body
    int pr = s / 14;                                       // pip radius
    if (pr < 1) pr = 1;
    int colA = x + s * 5 / 16, colB = x + s * 11 / 16;     // pip columns
    int rowA = y + s * 5 / 16, rowB = y + s * 11 / 16;     // pip rows (mirror of columns)
    int cx = x + s / 2, cy = y + s / 2;
    c.fillCircle(colA, rowA, pr, INK);                     // top-left
    c.fillCircle(colB, rowA, pr, INK);                     // top-right
    c.fillCircle(colA, rowB, pr, INK);                     // bottom-left
    c.fillCircle(colB, rowB, pr, INK);                     // bottom-right
    c.fillCircle(cx,   cy,   pr, INK);                     // center (5 pips)
}

// ---- per-game menu glyphs ----
static void glyphTicTac(M5EPD_Canvas& c, int x, int y, int s) {
    // X on the left, O on the right — reads instantly as крестики-нолики
    int ax0 = x + s / 12, ax1 = x + s * 5 / 12;
    int ay0 = y + s / 4,  ay1 = y + s * 3 / 4;
    c.drawLine(ax0, ay0, ax1, ay1, INK);
    c.drawLine(ax1, ay0, ax0, ay1, INK);
    c.drawLine(ax0, ay0 + 1, ax1, ay1 + 1, INK);          // 2px stroke
    c.drawLine(ax1, ay0 + 1, ax0, ay1 + 1, INK);
    int ocx = x + s * 3 / 4, ocy = y + s / 2, orr = s / 5;
    c.drawCircle(ocx, ocy, orr,     INK);
    c.drawCircle(ocx, ocy, orr - 1, INK);
}

static void glyphMine(M5EPD_Canvas& c, int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2, r = s / 4;
    for (int a = 0; a < 360; a += 45) {                    // spikes
        float rad = a * 3.14159265f / 180.0f;
        int x1 = cx + (int)(r * cosf(rad)),           y1 = cy + (int)(r * sinf(rad));
        int x2 = cx + (int)((r + s / 6) * cosf(rad)), y2 = cy + (int)((r + s / 6) * sinf(rad));
        c.drawLine(x1, y1, x2, y2, INK);
    }
    c.fillCircle(cx, cy, r, INK);                          // body
    c.fillCircle(cx - r / 3, cy - r / 3, r / 5, 0);        // white highlight
}

static void glyphFifteen(M5EPD_Canvas& c, int x, int y, int s) {
    // 4x4 grid of 15 filled tiles with the bottom-right cell empty — the canonical
    // sliding-puzzle look (16 cells, 15 tiles + 1 hole = «пятнашки»).
    int m = s / 8, bw = s - 2 * m, bx = x + m, by = y + m, cell = bw / 4;
    c.drawRect(bx, by, bw, bw, INK);
    for (int i = 1; i < 4; ++i) {
        c.drawLine(bx + i * cell, by, bx + i * cell, by + bw, INK);
        c.drawLine(bx, by + i * cell, bx + bw, by + i * cell, INK);
    }
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            if (row == 3 && col == 3) continue;           // the sliding hole
            c.fillRect(bx + col * cell + 2, by + row * cell + 2, cell - 4, cell - 4, INK);
        }
}

static void glyphSudoku(M5EPD_Canvas& c, int x, int y, int s) {
    int m = s / 8, bw = s - 2 * m, bx = x + m, by = y + m, cell = bw / 3;
    c.drawRect(bx, by, bw, bw, INK);
    c.drawRect(bx + 1, by + 1, bw - 2, bw - 2, INK);       // thicker border (vs tic-tac-toe)
    for (int i = 1; i < 3; ++i) {
        c.drawLine(bx + i * cell, by, bx + i * cell, by + bw, INK);
        c.drawLine(bx, by + i * cell, bx + bw, by + i * cell, INK);
    }
    int pr = s / 22; if (pr < 1) pr = 1;                   // "digit" dots on the diagonal
    c.fillCircle(bx + cell / 2,            by + cell / 2,            pr, INK);
    c.fillCircle(bx + cell + cell / 2,     by + cell + cell / 2,     pr, INK);
    c.fillCircle(bx + 2 * cell + cell / 2, by + 2 * cell + cell / 2, pr, INK);
}

void IconKit::app(M5EPD_Canvas& c, const std::string& id, int x, int y, int s) {
    if      (id == "reader")     glyphBook(c, x, y, s);
    else if (id == "ha")         house(c, x, y, s);
    else if (id == "weather")    glyphCloudSun(c, x, y, s);
    else if (id == "settings")   glyphGear(c, x, y, s);
    else if (id == "fileserver") glyphFolder(c, x, y, s);
    else if (id == "games")            glyphDice(c, x, y, s);
    else if (id == "game_tictactoe")   glyphTicTac(c, x, y, s);
    else if (id == "game_minesweeper") glyphMine(c, x, y, s);
    else if (id == "game_fifteen")     glyphFifteen(c, x, y, s);
    else if (id == "game_sudoku")      glyphSudoku(c, x, y, s);
    else                               glyphTile(c, x, y, s);
}

} // namespace paperos
