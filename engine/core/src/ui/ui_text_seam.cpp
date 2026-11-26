// The UI's text seam (ui_text_seam.hpp) and a label's wrapped lines.

#include <cstring>

#include "text/utf8.hpp"
#include "ui/ui_block.hpp"
#include "ui/ui_number.hpp"
#include "ui/ui_text_seam.hpp"
#include "ui/ui_wrap.hpp"

namespace nnh::detail {

char32_t seam_next_slow(const char*& p) { return next_codepoint(p); }

TextSize seam_measure(const char* utf8) {
    TextSize size;
    if (utf8 == nullptr || *utf8 == 0) return size;
    size.lines = 1;
    int cells = 0;
    const char* p = utf8;
    for (char32_t c = seam_next(p); c != 0; c = seam_next(p)) {
        if (c == U'\n') {
            ++size.lines;
            cells = 0;
        } else if (c != U'\r') {
            ++cells;
            ++size.total;
            if (cells > size.cells) size.cells = cells;
        }
    }
    return size;
}

int value_text(UiBlock& b, int s, int value, char* out, int capacity) {
    const bool progress = kind_of(b, s) == UiKind::Progress;
    const std::uint8_t slot = progress ? b.cold[s].data.progress.format : b.cold[s].data.slider.format;
    if (slot != kUiNoSlot) {
        const int n = fn_at<UiFormat>(b, slot)(value, out, capacity);
        return n < 0 ? 0 : (n > capacity ? capacity : n);
    }
    int n = ui_format_number(out, capacity, value);
    if (progress) {
        char max[16];
        const int m = ui_format_number(max, 16, b.cold[s].data.progress.max, "/");
        for (int i = 0; i < m && n < capacity; ++i) out[n++] = max[i];
    }
    return n;
}

int label_lines(UiBlock& b, int s, int width, int& cells) {
    const char* text = text_of(b, s);
    const UiLabelData& l = b.cold[s].data.label;
    const int cell = seam_cell(style_of(b, s).text);
    const int max_cells = (l.bits & 1) != 0 ? (width > 0 ? width / cell : 0) : 0;
    const int wrap_cells = (l.bits & 1) != 0 && max_cells < 1 ? 1 : max_cells;
    Wrapper w(text, static_cast<int>(std::strlen(text)), wrap_cells, l.max_lines);
    WrapLine line;
    int lines = 0;
    cells = 0;
    while (w.next(line)) {
        ++lines;
        cells += line.cells;
    }
    return lines;
}

} // namespace nnh::detail
