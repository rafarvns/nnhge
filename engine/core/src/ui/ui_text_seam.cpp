// The UI's text seam (ui_text_seam.hpp) and a label's wrapped lines.

#include <cstring>

#include "text/font_resolve.hpp"
#include "text/text_runs.hpp"
#include "text/text_wrap.hpp"
#include "text/utf8.hpp"
#include "ui/ui_block.hpp"
#include "ui/ui_number.hpp"
#include "ui/ui_text_seam.hpp"

namespace nnh::detail {

char32_t seam_next_slow(const char*& p) { return next_codepoint(p); }

TextFace seam_face(const UiBlock& b, const UiTextLook& look) {
    if (seam_builtin(look)) {
        TextFace face;
        face.scale = seam_scale(look);
        return face;
    }
    const int slot = static_cast<int>(look.font) - static_cast<int>(UiFont::Slot0);
    return face_for(slot >= 0 && slot < kMaxUiFonts ? b.fonts[slot] : FontRef{}, seam_scale(look));
}

int seam_line(const UiBlock& b, const UiTextLook& look) {
    if (seam_builtin(look)) return kFontLineHeight * seam_scale(look);
    const TextFace face = seam_face(b, look);
    return face_line_height(face) * face.scale;
}

TextSize seam_measure(const UiBlock& b, const UiTextLook& look, const char* utf8) {
    TextSize size;
    if (utf8 == nullptr || *utf8 == 0) return size;
    size.lines = 1;
    if (seam_builtin(look)) { // spec 028's walk: cells, then × cell once
        int cells = 0, widest = 0;
        const char* p = utf8;
        for (char32_t c = seam_next(p); c != 0; c = seam_next(p)) {
            if (c == U'\n') {
                ++size.lines;
                cells = 0;
            } else if (c != U'\r') {
                ++cells;
                ++size.total;
                if (cells > widest) widest = cells;
            }
        }
        size.width = widest * seam_cell(look);
        return size;
    }
    const TextFace face = seam_face(b, look);
    const char* line = utf8;
    for (const char* p = utf8;; ++p) {
        if (*p != '\n' && *p != 0) continue;
        const int w = run_width(face, line, p) * face.scale;
        size.width = w > size.width ? w : size.width;
        for (const char* q = line; q < p;) size.total += next_codepoint(q) != U'\r' ? 1 : 0;
        if (*p == 0) break;
        ++size.lines;
        line = p + 1;
    }
    return size;
}

int seam_width(const UiBlock& b, const UiTextLook& look, const char* utf8, int bytes) {
    if (utf8 == nullptr || bytes <= 0) return 0;
    if (seam_builtin(look)) {
        int cells = 0;
        for (const char* p = utf8; p < utf8 + bytes && *p != 0;) cells += seam_next(p) != U'\r' ? 1 : 0;
        return cells * seam_cell(look);
    }
    const TextFace face = seam_face(b, look);
    return run_width(face, utf8, utf8 + bytes) * face.scale;
}

int seam_fit(const UiBlock& b, const UiTextLook& look, const char* utf8, int width) {
    if (utf8 == nullptr) return 0;
    if (seam_builtin(look)) { // spec 028's cut: whole cells
        const int cells = width / seam_cell(look);
        const char* p = utf8;
        for (int i = 0; i < cells && *p != 0; ++i) next_codepoint(p);
        return static_cast<int>(p - utf8);
    }
    const TextFace face = seam_face(b, look);
    const char* end = utf8;
    while (*end != 0) ++end;
    int fit = static_cast<int>(end - utf8);
    bool stopped = false;
    walk_run(face, utf8, end, [&](const RunItem& it) {
        if (stopped || (it.pen + it.advance) * face.scale <= width) return;
        stopped = true;
        fit = it.byte; // the bytes before it fit
    });
    return fit;
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
    const UiTextLook& look = style_of(b, s).text;
    const TextFace face = seam_face(b, look);
    int wrap = 0;
    if ((l.bits & 1) != 0) {
        if (face.builtin()) { // spec 028: whole cells, at least one
            const int cell = seam_cell(look);
            const int max_cells = width > 0 ? width / cell : 0;
            wrap = (max_cells < 1 ? 1 : max_cells) * cell;
        } else {
            wrap = width > 0 ? width : 1;
        }
    }
    TextWrapper w(face, text, static_cast<int>(std::strlen(text)), wrap, l.max_lines);
    WrapLine line;
    int lines = 0;
    cells = 0;
    while (w.next(line)) {
        ++lines;
        cells += line.chars;
    }
    return lines;
}

} // namespace nnh::detail
