// The layout driver (spec 028 FR-13, D48): a root marked dirty is laid out by four linear walks — minimum widths
// bottom-up, widths top-down, wrapped heights bottom-up, heights and positions top-down (three when no label of the root
// wraps: its minimum heights come with the widths) — at the end of update(), on a
// rectangle query and at the start of draw() (the maintainer's answer to plan question 1). Never per frame otherwise.

#include "nnh/log.hpp"
#include "nnh/profile.hpp"
#include "ui/ui_layout.hpp"

NNH_LOG_CHANNEL_EXTERN(ui);

namespace nnh::detail {

Pad padding_of(const UiBlock& b, int s) {
    switch (kind_of(b, s)) {
    case UiKind::Root:
    case UiKind::Spacer: return {};
    case UiKind::VBox:
    case UiKind::HBox:
    case UiKind::GridBox:
    case UiKind::Stack:
    case UiKind::ScrollBox: {
        const int p = b.cold[s].data.box.padding;
        return {p, p, p, p};
    }
    default: {
        const std::uint8_t* p = style_of(b, s).padding;
        return {p[0], p[1], p[2], p[3]};
    }
    }
}

int grid_tracks(const UiBlock& b, int s, bool horizontal, std::int16_t* table) {
    const int cols = grid_columns(b, s);
    for (int k = 0; k < kGridTable; ++k) table[k] = 0;
    int col = 0, row = 0, count = 0;
    for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
        const int k = horizontal ? col : row;
        const int v = horizontal ? b.hot[c].min_w : b.hot[c].min_h;
        if (k < kGridTable && v > table[k]) table[k] = static_cast<std::int16_t>(v);
        ++count;
        if (++col == cols) {
            col = 0;
            ++row;
        }
    }
    return horizontal ? (count < cols ? count : cols) : row + (col > 0 ? 1 : 0);
}

int grid_track_at(const UiBlock& b, int s, bool horizontal, int k) {
    const int cols = grid_columns(b, s);
    int col = 0, row = 0, v = 0;
    for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
        const int m = horizontal ? b.hot[c].min_w : b.hot[c].min_h;
        if ((horizontal ? col : row) == k && m > v) v = m;
        if (++col == cols) {
            col = 0;
            ++row;
        }
    }
    return v;
}

void warn_overflow(UiBlock& b, int s) {
#if !defined(NNH_FLAVOR_RELEASE)
    UiHot& n = b.hot[s];
    if ((n.flags & kUiWarned) != 0) return;
    n.flags |= kUiWarned;
    NNH_LOG_WARN(ui, "widget %d (kind %d, key %d) overflows: its content needs more room than it has", s, n.kind,
                 n.key == 0xFFFF ? -1 : n.key);
#else
    (void)b;
    (void)s;
#endif
}

void layout_root(UiBlock& b, int root_index) {
    const int root = b.roots[root_index];
    b.dirty[root_index] = false;
    if (root == kUiNone) return;
    NNH_PROFILE_SCOPE("ui layout");
    // without a wrapping label no height depends on a width: heights are measured with the widths (3 walks, not 4)
    const bool folded = b.wrap_labels[root_index] == 0;
    measure_widths(b, root, folded);
    arrange_widths(b, root);
    if (!folded) measure_heights(b, root);
    arrange_heights(b, root);
    ++b.layouts;
}

void layout_if_dirty(UiBlock& b) {
    if (!b.dirty[0] && !b.dirty[1]) return;
    b.layout_visits = 0;
    for (int i = 0; i < 2; ++i) {
        if (b.dirty[i]) layout_root(b, i);
    }
}

} // namespace nnh::detail
