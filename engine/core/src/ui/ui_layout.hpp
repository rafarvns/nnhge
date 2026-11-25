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

/// Post-order over the visible nodes under `root` (`root` last): the first node, then each next (-1 at the end).
int postorder_first(const UiBlock& b, int root);
int postorder_next(const UiBlock& b, int root, int s);

/// The next visible sibling of `s` (s included when `self`), -1 when none.
inline int visible_from(const UiBlock& b, int s) {
    while (s != kUiNone && (b.hot[s].flags & kUiVisible) == 0) s = b.hot[s].next;
    return s == kUiNone ? -1 : s;
}
inline int first_visible_child(const UiBlock& b, int s) { return visible_from(b, b.hot[s].first_child); }
inline int next_visible_sibling(const UiBlock& b, int s) { return visible_from(b, b.hot[s].next); }

/// The box axis of a container: 0 none (stack, root, single panel), 1 vertical, 2 horizontal, 3 grid.
int box_axis(const UiBlock& b, int s);
/// GridBox columns.
inline int grid_columns(const UiBlock& b, int s) {
    const int c = b.cold[s].data.box.extra;
    return c < 1 ? 1 : c;
}
/// Spacing between a container's children.
inline int box_spacing(const UiBlock& b, int s) { return b.cold[s].data.box.spacing; }

/// Debug/Development: logs one warning per widget whose content doesn't fit (its kUiWarned flag).
void warn_overflow(UiBlock& b, int s);

/// Passes 1 and 3 (ui_layout_measure.cpp), 2 and 4 (ui_layout_arrange.cpp).
void measure_widths(UiBlock& b, int root);
void measure_heights(UiBlock& b, int root);
void arrange_widths(UiBlock& b, int root);
void arrange_heights(UiBlock& b, int root);

} // namespace nnh::detail
