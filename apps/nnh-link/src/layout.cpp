#include "layout.hpp"

namespace link_app::layout {
namespace {

bool continuation(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; } // UTF-8 10xxxxxx

} // namespace

int first_row(int selected) { return selected < kVisibleRows ? 0 : selected - kVisibleRows + 1; }

int row_at(int y, int selected, int rows) {
    if (y < kRowTop) return -1;
    const int on_screen = (y - kRowTop) / kRowHeight;
    const int row = first_row(selected) + on_screen;
    return on_screen < kVisibleRows && row < rows ? row : -1;
}

bool in_button(int x, int y) {
    return x >= kButtonX && x < kButtonX + kButtonW && y >= kButtonY && y < kButtonY + kButtonH;
}

std::string fit(const std::string& text, std::size_t max) {
    const std::size_t keep = max > 3 ? max - 3 : 0;
    std::size_t chars = 0;
    std::size_t cut = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (continuation(text[i])) continue;
        if (chars == keep) cut = i;
        if (++chars > max) return text.substr(0, cut) + "...";
    }
    return text;
}

std::vector<std::string> wrap(const std::vector<std::string>& lines, std::size_t width) {
    std::vector<std::string> rows;
    for (const std::string& line : lines) {
        std::string row;
        std::size_t chars = 0;
        for (const char c : line) {
            if (!continuation(c) && chars == width) {
                rows.push_back(row);
                row.clear();
                chars = 0;
            }
            row += c == '\t' ? ' ' : c;
            if (!continuation(c)) ++chars;
        }
        rows.push_back(row);
    }
    return rows;
}

} // namespace link_app::layout
