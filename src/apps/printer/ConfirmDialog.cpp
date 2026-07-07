#include "ConfirmDialog.h"
#include "framework/ui/Fonts.h"
#include "i18n/Strings.h"
#include "util/Utf8.h"
#include <vector>

namespace paperos {

namespace {

// Greedy word-wrap by codepoint count (NOT textWidth — smooth-font width
// under-reports cyrillic advance, see CLAUDE.md). Words longer than maxCp are
// hard-split at a codepoint boundary. Output capped at maxLines; if text
// remains after that, the last line's tail is replaced with an ellipsis.
std::vector<std::string> wrapMessage(const std::string& s, size_t maxCp, size_t maxLines) {
    std::vector<std::string> words;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') ++i;
        size_t start = i;
        while (i < s.size() && s[i] != ' ') ++i;
        if (i > start) words.push_back(s.substr(start, i - start));
    }

    std::vector<std::string> lines;
    std::string line;
    size_t lineCp = 0;
    for (auto& w : words) {
        size_t wCp = utf8CountCp(w.data(), w.size());
        if (wCp > maxCp) {
            // Hard-split an overlong word across as many lines as needed.
            if (!line.empty()) { lines.push_back(line); line.clear(); lineCp = 0; }
            size_t pos = 0;
            while (pos < w.size()) {
                size_t next = utf8AdvanceCp(w.data(), w.size(), pos, maxCp);
                lines.push_back(w.substr(pos, next - pos));
                pos = next;
            }
            continue;
        }
        size_t addCp = wCp + (line.empty() ? 0 : 1);
        if (lineCp + addCp > maxCp) {
            lines.push_back(line);
            line = w;
            lineCp = wCp;
        } else {
            if (!line.empty()) { line += ' '; }
            line += w;
            lineCp += addCp;
        }
    }
    if (!line.empty()) lines.push_back(line);

    if (lines.size() > maxLines) {
        lines.resize(maxLines);
        lines[maxLines - 1] += "\xE2\x80\xA6";
    }
    return lines;
}

} // namespace

void ConfirmDialog::open(ConfirmKind kind, const std::string& message,
                         const std::string& payload) {
    kind_ = kind;
    message_ = message;
    payload_ = payload;
}

void ConfirmDialog::render(M5EPD_Canvas& c) const {
    if (!active()) return;
    // Box: paper fill + ink border.
    c.fillRect(kBoxX, kBoxY, kBoxW, kBoxH, 0);
    c.drawRect(kBoxX, kBoxY, kBoxW, kBoxH, 15);
    c.drawRect(kBoxX + 1, kBoxY + 1, kBoxW - 2, kBoxH - 2, 15);

    Fonts fonts;
    fonts.apply(c, FontFace::Serif, 28);
    c.setTextColor(15);
    auto lines = wrapMessage(message_, 26, 3);
    for (size_t i = 0; i < lines.size(); ++i) {
        c.drawString(lines[i].c_str(), kBoxX + 24, kBoxY + 28 + static_cast<int>(i) * 38);
    }

    // Buttons.
    c.drawRect(kNoX,  kBtnY, kBtnW, kBtnH, 15);
    c.drawRect(kYesX, kBtnY, kBtnW, kBtnH, 15);
    fonts.apply(c, FontFace::Serif, 30);
    c.drawString(tr(Str::dlg_no),  kNoX + 60,  kBtnY + 28);
    c.drawString(tr(Str::dlg_yes), kYesX + 60, kBtnY + 28);
}

ConfirmDialog::Hit ConfirmDialog::hitTest(int x, int y) const {
    if (!active()) return Hit::None;
    if (y >= kBtnY && y <= kBtnY + kBtnH) {
        if (x >= kNoX  && x <= kNoX  + kBtnW) return Hit::No;
        if (x >= kYesX && x <= kYesX + kBtnW) return Hit::Yes;
    }
    return Hit::None;
}

} // namespace paperos
