// Navigation (spec 028 FR-16, D48): a direction is resolved in a fixed order — (1) the focused widget's own use (a
// slider or choice takes left/right, a list or grid moves its item), (2) an explicit neighbour, (3) the ancestors: the
// first box whose axis matches steps to its next target child (grid boxes by columns), wrapping at its end only on a
// fresh press unless it wraps on repeat, (4) the geometric rule within the root or the open modal, (5) blocked.

#include "ui/ui_focus.hpp"
#include "ui/ui_geometric.hpp"
#include "ui/ui_layout.hpp"

namespace nnh::detail {
namespace {

bool vertical(UiDir d) { return d == UiDir::Up || d == UiDir::Down; }
bool forward(UiDir d) { return d == UiDir::Down || d == UiDir::Right; }

/// The axis of box `p` matches `d`.
bool axis_matches(const UiBlock& b, int p, UiDir d) {
    const int axis = box_axis(b, p);
    return axis == 3 || (axis == 1 && vertical(d)) || (axis == 2 && !vertical(d));
}

/// The visible children of `p` in order: index of `child` and the count.
int index_of(const UiBlock& b, int p, int child, int& count) {
    int i = 0, at = -1;
    for (int c = first_visible_child(b, p); c >= 0; c = next_visible_sibling(b, c), ++i) {
        if (c == child) at = i;
    }
    count = i;
    return at;
}

int child_at(const UiBlock& b, int p, int index) {
    int i = 0;
    for (int c = first_visible_child(b, p); c >= 0; c = next_visible_sibling(b, c), ++i) {
        if (i == index) return c;
    }
    return -1;
}

/// One step of box `p` from its child `child`: the target reached, -1 when none.
int step_in_box(UiBlock& b, int p, int child, UiDir d, bool fresh) {
    int count = 0;
    const int at = index_of(b, p, child, count);
    if (at < 0 || count == 0) return -1;
    const bool grid = box_axis(b, p) == 3;
    const int cols = grid ? grid_columns(b, p) : 1;
    int step = forward(d) ? 1 : -1;
    if (grid && vertical(d)) step *= cols;
    const bool wraps = (b.cold[p].data.box.bits & 1) != 0 && (fresh || (b.cold[p].data.box.bits & 2) != 0);
    const bool scroll = kind_of(b, p) == UiKind::ScrollBox;
    b.ignore_window = scroll; // a scroll box's step reaches children outside its window, then reveals them
    int result = -1;
    int i = at;
    for (int tries = 0; tries < count; ++tries) {
        int next = i + step;
        if (grid && !vertical(d) && next / cols != at / cols) next = -1; // left/right stay in the row
        if (next < 0 || next >= count) {
            if (!wraps) break;
            if (grid && !vertical(d)) {
                next = forward(d) ? (at / cols) * cols : ((at / cols) * cols + cols - 1 < count ? (at / cols) * cols + cols - 1 : count - 1);
            } else if (grid) {
                next = forward(d) ? at % cols : ((count - 1 - at % cols) / cols) * cols + at % cols;
            } else {
                next = forward(d) ? 0 : count - 1;
            }
        }
        if (next == at) break;
        i = next;
        const int t = enter_target(b, child_at(b, p, i));
        if (t >= 0) {
            result = t;
            break;
        }
    }
    b.ignore_window = false;
    if (result >= 0 && scroll) scroll_reveal(b, p, child_at(b, p, i));
    return result;
}

/// The rectangle navigation starts from: a list's focused item, else the widget.
UiRect from_rect(const UiBlock& b, int s) {
    if (is_list(kind_of(b, s))) {
        const UiRect r = list_item_rect(b, s, b.cold[s].data.list.focused);
        if (r.w > 0) return r;
    }
    return rect_of(b.hot[s]);
}

int geometric(const UiBlock& b, int f, UiDir d) {
    const int root = b.modal_depth > 0 ? b.modals[b.modal_depth - 1].container : b.roots[root_index_of(b, f)];
    const UiRect from = from_rect(b, f);
    int best = -1;
    GeoKey best_key;
    for (int n = root; n >= 0; n = preorder_next_visible(b, root, n)) {
        if (n == f || !is_target(b, n)) continue;
        GeoKey k;
        if (!geo_candidate(from, rect_of(b.hot[n]), d, b.hot[n].order, k)) continue;
        if (best < 0 || geo_better(k, best_key)) {
            best = n;
            best_key = k;
        }
    }
    return best;
}

} // namespace

void navigate(UiBlock& b, UiDir d, bool fresh) {
    if (b.focus == kUiNone) {
        const int t = enter_target(b, b.modal_depth > 0 ? b.modals[b.modal_depth - 1].container
                                                        : b.roots[screen_index(b.first_focus)]);
        if (t >= 0) set_focus(b, t, true);
        return;
    }
    const int f = b.focus;
    const UiKind k = kind_of(b, f);
    // (1) the widget's own use
    if (is_list(k) && list_direction(b, f, d, fresh)) return;
    if ((k == UiKind::Slider || k == UiKind::Choice || k == UiKind::Toggle) && value_direction(b, f, d, fresh)) return;
    // (2) an explicit neighbour (the only way to the other root)
    const std::uint16_t n = b.cold[f].neighbours[static_cast<int>(d) & 3];
    if (n != kUiNone) {
        const int t = enter_target(b, n);
        if (t >= 0) {
            set_focus(b, t, true);
            return;
        }
    }
    // (3) the ancestors' boxes along their axis
    const int scope = b.modal_depth > 0 ? b.modals[b.modal_depth - 1].container : -1;
    for (int child = f, p = b.hot[f].parent; p != kUiNone && child != scope; child = p, p = b.hot[p].parent) {
        if (!axis_matches(b, p, d)) continue;
        const int t = step_in_box(b, p, child, d, fresh);
        if (t >= 0) {
            set_focus(b, t, true);
            return;
        }
    }
    // (4) the geometric rule, (5) blocked
    const int t = geometric(b, f, d);
    if (t >= 0) {
        set_focus(b, t, true);
        return;
    }
    add_result(b, UiResultKind::Blocked, f, static_cast<int>(d));
}

} // namespace nnh::detail
