#include "Reader.h"
#include "hal/Display.h"
#include "services/BookIndex.h"
#include "services/ConfigStore.h"
#include "framework/ui/Fonts.h"
#include "util/Logger.h"
#include "util/Utf8.h"
#include "i18n/Strings.h"
#include <Arduino.h>

namespace paperos {

namespace {

// Maps font_size index (0=S,1=M,2=L) to pixel size used by Fonts.apply().
int pixelSizeFor(uint8_t font_idx) {
    switch (font_idx) { case 0: return 22; case 2: return 30; default: return 26; }
}

// Append `cp` to `out` as UTF-8 bytes (1-3 byte sequence; ≤0xFFFF only).
inline void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// How many bytes one UTF-8 codepoint occupies starting at byte `b`.
inline size_t utf8ByteLen(uint8_t b) {
    if ((b & 0x80) == 0) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    return 4;
}

constexpr size_t kReadAhead = 16 * 1024;        // bytes per page render
constexpr int    kStatusReserve = 40;            // px reserved at bottom

} // anonymous

void Reader::enter(AppContext& ctx) {
    std::string last = ctx.config.lastBookPath();
    if (!last.empty()) {
        uint32_t off = ctx.config.lastBookOffset();
        openBook(last, off, ctx);
    } else {
        mode_ = Mode::BookList;
        renderBookList(ctx);
    }
}

void Reader::leave(AppContext& ctx) {
    if (mode_ == Mode::Reading) saveBookmark(ctx);
}

void Reader::tick(AppContext& ctx) {
    if (mode_ == Mode::Reading) {
        uint32_t now = millis();
        if (now - last_save_ms_ > 30000) {
            saveBookmark(ctx);
            last_save_ms_ = now;
        }
    }
}

void Reader::renderBookList(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    Fonts fonts; fonts.apply(c, FontFace::Serif, kHeaderFontPx);
    c.setTextColor(15);
    c.drawString(tr(Str::app_reader), kHeaderTextX, kHeaderTextY);

    fonts.apply(c, FontFace::Serif, 24);
    const auto& books = ctx.books.books();
    if (books.empty()) {
        c.drawString(tr(Str::reader_no_books), 30, 100);
    } else {
        constexpr int16_t row_h = 60;
        for (size_t i = 0; i < books.size(); ++i) {
            int16_t y = 100 + static_cast<int16_t>(i) * row_h - scroll_offset_;
            if (y > kScreenH || y + row_h < 0) continue;
            c.drawString(books[i].title.c_str(), 30, y);
            c.drawLine(20, y + row_h - 5, kScreenW - 20, y + row_h - 5, 8);
        }
    }
    ctx.display.pushRegion({0,0,kScreenW,kScreenH}, PushMode::Full);
}

void Reader::onBookListTouch(int16_t x, int16_t y, AppContext& ctx) {
    (void)x;
    constexpr int16_t row_h = 60;
    const auto& books = ctx.books.books();
    if (y < 100) return;
    int idx = (y - 100 + scroll_offset_) / row_h;
    if (idx >= 0 && idx < static_cast<int>(books.size())) {
        openBook(books[idx].path, 0, ctx);
    }
}

void Reader::openBook(const std::string& path, uint32_t offset, AppContext& ctx) {
    current_path_ = path;
    enc_ = ctx.books.detectEncoding(path.c_str());
    page_offsets_.clear();
    page_offsets_.push_back(offset);
    current_page_ = 0;
    at_end_of_book_ = false;
    last_save_ms_ = millis();

    // Look up book size for the % progress footer (BookIndex already cached it
    // during its scan, so this is just an in-RAM lookup).
    book_size_ = 0;
    for (const auto& e : ctx.books.books()) {
        if (e.path == path) { book_size_ = static_cast<uint32_t>(e.size_bytes); break; }
    }

    mode_ = Mode::Reading;
    renderPage(ctx);
}

void Reader::renderPage(AppContext& ctx) {
    auto& c = ctx.display.canvas();
    c.fillCanvas(0);
    int px = pixelSizeFor(ctx.config.fontSize());
    int margin = ctx.config.marginPx();
    Fonts fonts; fonts.apply(c, FontFace::Serif, px);
    c.setTextColor(15);

    uint32_t off = page_offsets_[current_page_];
    std::vector<uint8_t> buf(kReadAhead);
    size_t got = ctx.books.readChunk(current_path_.c_str(), off, buf.data(), buf.size());
    buf.resize(got);

    // M5EPD/TFT_eSPI smooth-font drawString works reliably on whole strings;
    // calling it per codepoint (with textWidth() for advance) misreports widths
    // for UTF-8 multibyte chars and wraps every glyph to a new line. We accumulate
    // word-by-word into `line` and emit one drawString per line.
    //
    // We also track `last_safe_pos` — the byte position immediately after the
    // last word that was successfully drawn. The next page MUST start there
    // (not at some PageBreaker-predicted offset), so text never gets skipped
    // between pages.
    const int line_h = px + 8;
    // Safety pad: M5EPD's smooth-font textWidth() under-reports the rendered
    // advance for Cyrillic (especially capitals — DejaVu Serif uppercase is
    // visibly wider than the measurement returns), so the last char on a tight
    // line can clip past the right margin. Pull the fit-boundary in by ~¾ of
    // the font's pixel size — generous, but caps still wrap predictably.
    const int width_safety = px * 3 / 4;
    const int content_right = kScreenW - margin - width_safety;
    const int max_y = kScreenH - margin - kStatusReserve;
    int y = margin;

    std::string line;
    std::string word;
    size_t pos = 0;
    size_t word_start_pos = 0;   // byte position where current `word` started
    size_t last_safe_pos = 0;    // bytes consumed by everything successfully drawn
    bool overflow = false;

    auto flushLine = [&](size_t pos_after_line) {
        if (!line.empty()) {
            c.drawString(line.c_str(), margin, y);
            line.clear();
            last_safe_pos = pos_after_line;
        }
        y += line_h;
    };

    auto lineWouldFit = [&](const std::string& extra) -> bool {
        std::string candidate = line + extra;
        int w = c.textWidth(candidate.c_str());
        // Fallback if smooth-font width is nonsensical for this combination of
        // glyphs: estimate by codepoint count.
        if (w <= 0 || w > kScreenW * 4) {
            int cps = 0;
            for (size_t p = 0; p < candidate.size(); ) {
                p += utf8ByteLen(static_cast<uint8_t>(candidate[p]));
                cps++;
            }
            w = cps * px * 55 / 100;
        }
        return margin + w <= content_right;
    };

    while (pos < buf.size()) {
        if (y + line_h > max_y) { overflow = true; break; }

        size_t cp_start = pos;
        uint32_t cp = (enc_ == Encoding::Utf8) ? utf8Next(buf.data(), buf.size(), &pos)
                                                : cp1251ToCodepoint(buf[pos++]);
        if (cp == 0) break;
        if (cp == '\r') continue;

        if (cp == '\n') {
            if (!word.empty()) {
                if (!lineWouldFit(word)) flushLine(word_start_pos);
                line += word;
                word.clear();
            }
            flushLine(pos);   // pos is just past '\n'
            continue;
        }

        if (cp == ' ' || cp == '\t') {
            if (!word.empty()) {
                if (!lineWouldFit(word)) flushLine(word_start_pos);
                line += word;
                word.clear();
            }
            if (!line.empty()) line.push_back(' ');
            continue;
        }

        if (word.empty()) word_start_pos = cp_start;
        appendUtf8(word, cp);

        // Defensive: if a single "word" (no spaces) is already wider than the
        // line on its own — break it now to avoid runaway accumulation. Drops
        // the last codepoint and starts the next line with it.
        if (line.empty() && !lineWouldFit(word)) {
            size_t last_cp_start = word.size();
            while (last_cp_start > 0 &&
                   (static_cast<uint8_t>(word[last_cp_start - 1]) & 0xC0) == 0x80) {
                last_cp_start--;
            }
            if (last_cp_start > 0) last_cp_start--;
            std::string tail = word.substr(last_cp_start);
            word.resize(last_cp_start);
            line = word;
            word = tail;
            flushLine(cp_start);     // safe up to the codepoint that didn't fit
            word_start_pos = cp_start;
        }
    }

    // Drain leftovers — only if we still have screen room. If we overflowed
    // the page, we deliberately do NOT draw the unflushed `line` / `word`:
    // they will be redrawn at the top of the next page.
    if (!overflow) {
        if (!word.empty()) {
            if (!lineWouldFit(word)) flushLine(word_start_pos);
            line += word;
            word.clear();
        }
        if (!line.empty() && y + line_h <= max_y) flushLine(pos);
    }

    // If we drew nothing at all, last_safe_pos stays 0 — treat as EOF.
    if (last_safe_pos == 0) {
        at_end_of_book_ = true;
    } else {
        at_end_of_book_ = (got < kReadAhead) && (last_safe_pos >= got);
        // Record where the next page begins.
        uint32_t next_off = off + static_cast<uint32_t>(last_safe_pos);
        if (current_page_ + 1 >= page_offsets_.size()) {
            page_offsets_.push_back(next_off);
        } else {
            page_offsets_[current_page_ + 1] = next_off;
            // Invalidate any further entries — they were computed against a stale
            // boundary and would now be wrong.
            page_offsets_.resize(current_page_ + 2);
        }
    }

    // Status line — show % progress through the book (works for any starting
    // offset, no full index required). Falls back to page number if book size
    // is unknown.
    fonts.apply(c, FontFace::Serif, 18);
    char status[32];
    if (book_size_ > 0) {
        uint32_t pos_bytes = page_offsets_[current_page_];
        unsigned pct = static_cast<unsigned>((static_cast<uint64_t>(pos_bytes) * 100) / book_size_);
        if (pct > 100) pct = 100;
        snprintf(status, sizeof(status), "%u%%", pct);
    } else {
        snprintf(status, sizeof(status), tr(Str::reader_page_fmt),
                 static_cast<unsigned>(current_page_ + 1));
    }
    c.drawString(status, kScreenW - 200, kScreenH - 30);

    ctx.display.pushRegion({0,0,kScreenW,kScreenH}, PushMode::Partial);
}

void Reader::onInput(const InputEvent& e, AppContext& ctx) {
    if (mode_ == Mode::BookList) {
        if      (e.kind == InputEvent::Kind::TouchUp) onBookListTouch(e.x, e.y, ctx);
        else if (e.kind == InputEvent::Kind::NavUp)   scrollBookList(-1, ctx);
        else if (e.kind == InputEvent::Kind::NavDown) scrollBookList(+1, ctx);
        return;
    }
    // Reading mode
    if      (e.kind == InputEvent::Kind::TouchUp) onReadingTouch(e.x, e.y, ctx);
    else if (e.kind == InputEvent::Kind::NavUp)   stepPageBack(ctx);
    else if (e.kind == InputEvent::Kind::NavDown) stepPageForward(ctx);
}

bool Reader::onBack(AppContext& ctx) {
    if (mode_ == Mode::Reading) {
        saveBookmark(ctx);
        mode_ = Mode::BookList;
        renderBookList(ctx);
        return true;            // consumed: dropped from reading to the list
    }
    return false;               // BookList is the app's top level → Launcher
}

void Reader::scrollBookList(int dir, AppContext& ctx) {
    // One screenful minus one row of overlap. row_h matches renderBookList (60),
    // list starts at y=100, so the visible window is (kScreenH - 100) tall.
    constexpr int row_h = 60;
    const int visible_h = kScreenH - 100;
    const int step = (visible_h - row_h) * dir;
    const int n = static_cast<int>(ctx.books.books().size());
    const int content_h = n * row_h;
    const int max_off = (content_h > visible_h) ? (content_h - visible_h) : 0;
    int next = scroll_offset_ + step;
    if (next < 0) next = 0;
    if (next > max_off) next = max_off;
    if (next == scroll_offset_) return;   // at edge — no refresh
    scroll_offset_ = next;
    renderBookList(ctx);
}

// One screen of body text is roughly 2 KB of UTF-8 Russian prose. When the
// reader was opened via a bookmark (page_offsets_[0] > 0) we don't have any
// prior page boundaries cached, so prev-on-page-0 has to synthesize one. We
// step back this many bytes and snap to the next '\n' so we don't start
// mid-word. Subsequent prev presses keep stepping further.
constexpr uint32_t kPrevHeuristicStep = 2048;

void Reader::stepPageBack(AppContext& ctx) {
    if (current_page_ > 0) {
        current_page_--;
        at_end_of_book_ = false;
        renderPage(ctx);
        return;
    }
    uint32_t cur_off = page_offsets_[0];
    if (cur_off == 0) return;  // already at book start

    uint32_t back = (cur_off > kPrevHeuristicStep) ? cur_off - kPrevHeuristicStep : 0;
    // Snap forward to the byte right after the first '\n' inside the gap, so
    // we don't begin rendering in the middle of a word. If there's no '\n'
    // (extreme: a 2 KB-long single line), we accept the unaligned start —
    // the page will visually look slightly off but navigation still works.
    if (back > 0 && cur_off > back) {
        std::vector<uint8_t> scan(cur_off - back);
        size_t got = ctx.books.readChunk(current_path_.c_str(), back, scan.data(), scan.size());
        for (size_t i = 0; i < got; i++) {
            if (scan[i] == '\n') { back += static_cast<uint32_t>(i + 1); break; }
        }
        if (back >= cur_off) back = (cur_off > kPrevHeuristicStep) ? cur_off - kPrevHeuristicStep : 0;
    }

    // Prepend the new start; current_page_ now refers to this synthesized
    // page. renderPage() will rewrite page_offsets_[1] with the actual end
    // of this page (which may differ slightly from the prior cur_off — the
    // bookmark-opening "history" was an approximation anyway).
    page_offsets_.insert(page_offsets_.begin(), back);
    current_page_ = 0;
    at_end_of_book_ = false;
    renderPage(ctx);
}

void Reader::stepPageForward(AppContext& ctx) {
    if (current_page_ + 1 < page_offsets_.size() && !at_end_of_book_) {
        current_page_++;
        renderPage(ctx);
    }
}

void Reader::onReadingTouch(int16_t x, int16_t y, AppContext& ctx) {
    (void)y;
    if (x < kScreenW / 3) {
        stepPageBack(ctx);
    } else if (x > kScreenW * 2 / 3) {
        stepPageForward(ctx);
    } else {
        // Center: return to book list.
        saveBookmark(ctx);
        mode_ = Mode::BookList;
        renderBookList(ctx);
    }
}

void Reader::saveBookmark(AppContext& ctx) {
    if (mode_ != Mode::Reading) return;
    ctx.config.setLastBookPath(current_path_);
    ctx.config.setLastBookOffset(page_offsets_[current_page_]);
}

} // namespace paperos
