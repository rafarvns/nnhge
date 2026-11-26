#pragma once
// Shared by the layout passes (ui_layout_measure.cpp, ui_layout_arrange.cpp): padding, walks over visible nodes
// without recursion, the overflow warning.

#include "ui/ui_block.hpp"

namespace nnh::detail {

struct Pad {
    int l = 0, t = 0, r = 0, b = 0;
};

/// A node's padding: boxes their own, panels and leaves their style's, roots and spacers none.
Pad padding_of(const UiBlock& b, int s);

/// The next visible sibling of `s` (s included when `self`), -1 when none.
inline int visible_from(const UiBlock& b, int s) {
    while (s != kUiNone && (b.hot[s].flags & kUiVisible) == 0) s = b.hot[s].next;
    return s == kUiNone ? -1 : s;
}
inline int first_visible_child(const UiBlock& b, int s) { return visible_from(b, b.hot[s].first_child); }
inline int next_visible_sibling(const UiBlock& b, int s) { return visible_from(b, b.hot[s].next); }

/// Post-order over the visible nodes under `root` (`root` last): the first node, then each next (-1 at the end).
/// Inline, as the walks' other steps: the layout passes call them once per node.
inline int deepest_visible(const UiBlock& b, int s) {
    for (int c = first_visible_child(b, s); c >= 0; c = first_visible_child(b, c)) s = c;
    return s;
}
inline int postorder_first(const UiBlock& b, int root) { return deepest_visible(b, root); }
inline int postorder_next(const UiBlock& b, int root, int s) {
    if (s == root) return -1;
    const int sibling = next_visible_sibling(b, s);
    return sibling >= 0 ? deepest_visible(b, sibling) : b.hot[s].parent;
}

/// The box axis of a container: 0 none (stack, root, single panel), 1 vertical, 2 horizontal, 3 grid, 4 a panel's one
/// child (fills).
inline int box_axis(const UiBlock& b, int s) {
    switch (kind_of(b, s)) {
    case UiKind::VBox:
    case UiKind::ScrollBox: return 1;
    case UiKind::HBox: return 2;
    case UiKind::GridBox: return 3;
    case UiKind::Panel: {
        const int box = b.cold[s].data.box.extra;
        return box == 1 ? 1 : (box == 2 ? 2 : 4);
    }
    default: return 0;
    }
}
/// GridBox columns.
inline int grid_columns(const UiBlock& b, int s) {
    const int c = b.cold[s].data.box.extra;
    return c < 1 ? 1 : c;
}
/// A grid box's track sizes (columns for widths, rows for heights): the largest child minimum of each, for the first
/// kGridTable tracks, in one walk without division. @return the number of tracks.
inline constexpr int kGridTable = 64;
int grid_tracks(const UiBlock& b, int s, bool horizontal, std::int16_t* table);
/// The size of track `k` found by a walk (tracks past the table).
int grid_track_at(const UiBlock& b, int s, bool horizontal, int k);

/// Spacing between a container's children.
inline int box_spacing(const UiBlock& b, int s) { return b.cold[s].data.box.spacing; }

/// Debug/Development: logs one warning per widget whose content doesn't fit (its kUiWarned flag).
void warn_overflow(UiBlock& b, int s);

/// Passes 1 and 3 (ui_layout_measure.cpp), 2 and 4 (ui_layout_arrange.cpp).
/// `heights`: also the minimum heights in the same walk (a root without wrapping labels: pass 3 isn't needed).
void measure_widths(UiBlock& b, int root, bool heights);
void measure_heights(UiBlock& b, int root);
void arrange_widths(UiBlock& b, int root);
void arrange_heights(UiBlock& b, int root);

} // namespace nnh::detail
