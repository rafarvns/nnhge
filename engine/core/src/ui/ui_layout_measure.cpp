// Layout passes 1 and 3 (spec 028 FR-9, FR-13, D48): minimum widths bottom-up (a wrapping label's is its longest
// word), then — after pass 2 gave every node its width — wrapped lines and minimum heights bottom-up. A fixed size
// replaces the minimum (smaller than the content: an overflow, warned once).

#include "ui/ui_layout.hpp"
#include "ui/ui_text_seam.hpp"
#include "ui/ui_wrap.hpp"

#include <cstring>

namespace nnh::detail {

const char* option_text(UiBlock& b, int i); // ui_text_source.cpp

namespace {

constexpr int kToggleBox = 10, kGap = 4, kTrackMin = 48;

int value_cells(UiBlock& b, int s, int v) {
    char buf[64];
    const int n = value_text(b, s, v, buf, 63);
    buf[n] = 0;
    return seam_measure(buf).cells;
}

/// Width of a choice or slider arrow / text part.
int arrow_w(const UiImagePart& part, const UiStyle& st) {
    if (part.texture >= 0) return part.region.w;
    return st.choice.text_arrows ? seam_cell(st.text) : 0;
}

int labelled(int text_cells, int cell) { return text_cells > 0 ? text_cells * cell + kGap : 0; }

int leaf_width(UiBlock& b, int s) {
    const UiStyle& st = style_of(b, s);
    const int cell = seam_cell(st.text);
    const UiCold& c = b.cold[s];
    const TextSize t = seam_measure(text_of(b, s));
    switch (kind_of(b, s)) {
    case UiKind::Label: {
        if ((c.data.label.bits & 1) == 0) return t.cells * cell;
        const char* text = text_of(b, s);
        return longest_word_cells(text, static_cast<int>(std::strlen(text))) * cell;
    }
    case UiKind::Image: return (c.data.image.rw > 0 ? c.data.image.rw : 0) * c.data.image.scale;
    case UiKind::Progress:
        return (c.data.progress.bits & 2) != 0 ? value_cells(b, s, c.data.progress.max) * cell : 0;
    case UiKind::Button: {
        const int icon = c.data.button.icon_texture >= 0 ? c.data.button.iw : 0;
        return icon + (icon > 0 && t.cells > 0 ? kGap : 0) + t.cells * cell;
    }
    case UiKind::Toggle: return kToggleBox + (t.cells > 0 ? kGap + t.cells * cell : 0);
    case UiKind::Slider: {
        const UiSliderData& x = c.data.slider;
        int w = labelled(t.cells, cell) + kTrackMin;
        if (x.show_value != 0) {
            const int a = value_cells(b, s, x.min), z = value_cells(b, s, x.max);
            w += kGap + (a > z ? a : z) * cell;
        }
        return w;
    }
    case UiKind::Choice: {
        int widest = 0;
        for (int i = 0; i < c.data.choice.count; ++i) {
            const int w = seam_measure(option_text(b, c.data.choice.first_text + i)).cells;
            if (w > widest) widest = w;
        }
        return labelled(t.cells, cell) + arrow_w(st.choice.left, st) + kGap + widest * cell + kGap +
               arrow_w(st.choice.right, st);
    }
    case UiKind::List:
    case UiKind::Grid: {
        const UiListData& l = c.data.list;
        return (l.cell_width > 0 ? l.cell_width : 64) * l.columns + st.list.scroll_width;
    }
    default: return 0;
    }
}

int leaf_height(UiBlock& b, int s) {
    const UiStyle& st = style_of(b, s);
    const int line = seam_line(st.text);
    const UiCold& c = b.cold[s];
    switch (kind_of(b, s)) {
    case UiKind::Label: return c.data.label.lines * line;
    case UiKind::Image: return (c.data.image.rh > 0 ? c.data.image.rh : 0) * c.data.image.scale;
    case UiKind::Progress: return (c.data.progress.bits & 2) != 0 ? line : 0; // a bar alone: its fixed size
    case UiKind::Button: {
        const int text = seam_measure(text_of(b, s)).lines * line;
        const int icon = c.data.button.icon_texture >= 0 ? c.data.button.ih : 0;
        return text > icon ? text : icon;
    }
    case UiKind::Toggle: return line > kToggleBox ? line : kToggleBox;
    case UiKind::Slider:
    case UiKind::Choice: return line;
    case UiKind::List:
    case UiKind::Grid: return c.data.list.rows * list_row_height(b, s);
    default: return 0;
    }
}

/// A container's content size along one axis from its children's minimums.
int container_size(const UiBlock& b, int s, bool horizontal) {
    const int axis = box_axis(b, s);
    const int spacing = box_spacing(b, s);
    int sum = 0, max = 0, n = 0;
    if (axis == 3) { // grid box: per column (width) or per row (height) maximums, one scan per track (no table)
        const int cols = grid_columns(b, s);
        int count = 0;
        for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) ++count;
        const int tracks = horizontal ? (count < cols ? count : cols) : (count + cols - 1) / cols;
        for (int k = 0; k < tracks; ++k) {
            int i = 0, track = 0;
            for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c), ++i) {
                const int v = horizontal ? b.hot[c].min_w : b.hot[c].min_h;
                if ((horizontal ? i % cols : i / cols) == k && v > track) track = v;
            }
            sum += track;
        }
        return sum + (tracks > 1 ? (tracks - 1) * spacing : 0);
    }
    const bool stacked = axis == 0;
    for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
        const UiHot& h = b.hot[c];
        int v = horizontal ? h.min_w : h.min_h;
        if (stacked) { // anchored: the offset (and a fill's far margin) count
            const UiCold& cc = b.cold[c];
            const int off = horizontal ? cc.px : cc.py;
            const bool fill = (cc.cflags & (horizontal ? kUiFillX : kUiFillY)) != 0;
            v += (off < 0 ? -off : off) + (fill ? (horizontal ? cc.pright : cc.pbottom) : 0);
        }
        sum += v;
        if (v > max) max = v;
        ++n;
    }
    const bool along = (axis == 1 && !horizontal) || (axis == 2 && horizontal);
    if (kind_of(b, s) == UiKind::ScrollBox) return max; // at least the largest child shows
    return along ? sum + (n > 1 ? (n - 1) * spacing : 0) : max;
}

void finish(UiBlock& b, int s, int content, bool horizontal) {
    UiHot& n = b.hot[s];
    const Pad p = padding_of(b, s);
    const int needed = content + (horizontal ? p.l + p.r : p.t + p.b);
    const int fixed = horizontal ? b.cold[s].fixed_w : b.cold[s].fixed_h;
    int v = needed;
    if (fixed > 0) {
        if (fixed < needed) warn_overflow(b, s);
        v = fixed;
    }
    v = v > 32767 ? 32767 : v;
    if (horizontal) {
        n.min_w = static_cast<std::int16_t>(v);
    } else {
        n.min_h = static_cast<std::int16_t>(v);
    }
}

} // namespace

void measure_widths(UiBlock& b, int root) {
    for (int s = postorder_first(b, root); s >= 0; s = postorder_next(b, root, s)) {
        ++b.layout_visits;
        const UiKind k = kind_of(b, s);
        finish(b, s, is_container(k) ? container_size(b, s, true) : leaf_width(b, s), true);
    }
}

void measure_heights(UiBlock& b, int root) {
    for (int s = postorder_first(b, root); s >= 0; s = postorder_next(b, root, s)) {
        ++b.layout_visits;
        const UiKind k = kind_of(b, s);
        if (k == UiKind::Label) {
            const Pad p = padding_of(b, s);
            int cells = 0;
            const int lines = label_lines(b, s, b.hot[s].w - p.l - p.r, cells);
            UiLabelData& l = b.cold[s].data.label;
            l.lines = static_cast<std::uint8_t>(lines > 255 ? 255 : lines);
            l.cells = static_cast<std::uint16_t>(cells > 65534 ? 65534 : cells);
        }
        finish(b, s, is_container(k) ? container_size(b, s, false) : leaf_height(b, s), false);
    }
}

} // namespace nnh::detail
