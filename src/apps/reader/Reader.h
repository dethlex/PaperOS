#pragma once
#include "framework/App.h"
#include "apps/reader/EncodingDetect.h"
#include "i18n/Strings.h"
#include <vector>
#include <string>

namespace paperos {

class Reader : public IApp {
public:
    void enter(AppContext& ctx) override;
    void leave(AppContext& ctx) override;
    void tick(AppContext& ctx) override;
    void onInput(const InputEvent& e, AppContext& ctx) override;
    bool onBack(AppContext& ctx) override;
    const char* id() const override { return "reader"; }
    const char* title() const override { return tr(Str::app_reader); }

private:
    enum class Mode { BookList, Reading };
    Mode mode_ = Mode::BookList;

    // BookList state
    int scroll_offset_ = 0;
    void renderBookList(AppContext& ctx);
    void onBookListTouch(int16_t x, int16_t y, AppContext& ctx);
    void scrollBookList(int dir, AppContext& ctx);   // G37/G39 in BookList: dir = -1/+1

    // Reading state.
    // page_offsets_ is built on-the-fly by renderPage(): each call records the
    // exact byte where the next page should start. Forward navigation always
    // re-renders the next page and extends the array; backward uses the stored
    // start offset of the previous page. This avoids relying on a separate
    // pagination pass whose width metrics drift away from what the smooth-font
    // renderer actually places on screen (we'd otherwise see skipped text
    // between pages because PageBreaker measured glyphs differently).
    std::string current_path_;
    Encoding enc_ = Encoding::Utf8;
    std::vector<uint32_t> page_offsets_;     // [0] = start; [i+1] = end of page i
    size_t current_page_ = 0;
    bool at_end_of_book_ = false;            // true when last render hit EOF
    uint32_t book_size_ = 0;                 // bytes; for % progress in footer
    uint32_t last_save_ms_ = 0;

    void openBook(const std::string& path, uint32_t offset, AppContext& ctx);
    void renderPage(AppContext& ctx);
    void stepPageBack(AppContext& ctx);     // honors heuristic when no history
    void stepPageForward(AppContext& ctx);
    void onReadingTouch(int16_t x, int16_t y, AppContext& ctx);
    void saveBookmark(AppContext& ctx);
};

} // namespace paperos
