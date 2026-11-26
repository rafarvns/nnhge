// Scroll boxes (spec 028 FR-12): ordinary children stacked vertically, a window of whole children inside the box's
// height; moving the focus to a child outside it scrolls (with the margin of children kept visible); scrolling moves
// the children's rectangles (ui_layout_arrange.cpp's layout_scroll_window), never a relayout.

#include "ui/ui_layout.hpp"

namespace nnh::detail {
namespace {

int child_index(const UiBlock& b, int sb, int child) {
    int i = 0;
    for (int c = first_visible_child(b, sb); c >= 0; c = next_visible_sibling(b, c), ++i) {
        if (c == child) return i;
    }
    return -1;
}

int child_height(const UiBlock& b, int sb, int index) {
    int i = 0;
    for (int c = first_visible_child(b, sb); c >= 0; c = next_visible_sibling(b, c), ++i) {
        if (i == index) return b.hot[c].h;
    }
    return 0;
}

/// The children fully inside the window when `first` is the first one shown (at least 1).
int fitting(const UiBlock& b, int sb, int first, int count) {
    const Pad p = padding_of(b, sb);
    const int window = b.hot[sb].h - p.t - p.b;
    const int spacing = box_spacing(b, sb);
    int used = 0, n = 0;
    for (int i = first; i < count; ++i) {
        const int h = child_height(b, sb, i);
        if (n > 0 && used + spacing + h > window) break;
        if (n == 0 && h > window) return 1;
        used += (n > 0 ? spacing : 0) + h;
        ++n;
    }
    return n < 1 ? 1 : n;
}

} // namespace

int scroll_child_count(const UiBlock& b, int sb) {
    int n = 0;
    for (int c = first_visible_child(b, sb); c >= 0; c = next_visible_sibling(b, c)) ++n;
    return n;
}

void scroll_set_first(UiBlock& b, int sb, int first) {
    const int count = scroll_child_count(b, sb);
    const int f = first < 0 ? 0 : (first >= count ? (count > 0 ? count - 1 : 0) : first);
    UiBoxData& x = b.cold[sb].data.box;
    if (x.first == f) return;
    x.first = static_cast<std::uint16_t>(f);
    layout_scroll_window(b, sb);
}

void scroll_reveal(UiBlock& b, int sb, int child) {
    const int count = scroll_child_count(b, sb);
    const int i = child_index(b, sb, child);
    if (i < 0) return;
    const int margin = b.cold[sb].data.box.extra;
    int first = b.cold[sb].data.box.first;
    const int lo = i - margin < 0 ? 0 : i - margin;
    const int hi = i + margin >= count ? count - 1 : i + margin;
    if (lo < first) first = lo;
    while (first < i && first + fitting(b, sb, first, count) - 1 < hi) ++first;
    scroll_set_first(b, sb, first);
}

} // namespace nnh::detail
