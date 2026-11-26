// Layout passes 2 and 4 (spec 028 FR-9, FR-10, FR-12, D48): widths and x top-down, then heights and y top-down, in
// screen pixels. Boxes share extra space by stretch ratios along their axis and align across it; roots and stacks place
// children by anchors; a scroll box stacks its children and moves them up by its scroll (whole children), marking
// which lie inside its window. Pre-order indexes are refreshed.

#include "ui/ui_anchor.hpp"
#include "ui/ui_box_math.hpp"
#include "ui/ui_layout.hpp"

namespace nnh::detail {
namespace {

std::int16_t i16(int v) { return static_cast<std::int16_t>(v < -32768 ? -32768 : (v > 32767 ? 32767 : v)); }

UiAlign align_of(const UiHot& n) { return static_cast<UiAlign>(n.align & 3u); }
bool expands(const UiHot& n) { return (n.align & kUiExpandBit) != 0; }

UiPlace place_of(const UiCold& c) {
    return UiPlace{.anchor = static_cast<UiAnchor>(c.anchor), .x = c.px, .y = c.py,
                   .fill_x = (c.cflags & kUiFillX) != 0, .fill_y = (c.cflags & kUiFillY) != 0,
                   .right = c.pright, .bottom = c.pbottom};
}

/// Main-axis sizes of a box's visible children (into w or h). @return the total (spacing included).
int distribute(UiBlock& b, int s, bool horizontal, int available) {
    const int spacing = box_spacing(b, s);
    // one walk: every child at its minimum, which is the whole answer when none expands (the common case)
    int n = 0, sum = 0;
    bool any_expand = false;
    for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
        UiHot& h = b.hot[c];
        const std::int16_t min = horizontal ? h.min_w : h.min_h;
        (horizontal ? h.w : h.h) = min;
        h.flags = static_cast<std::uint8_t>(h.flags & ~kUiScratch);
        sum += min;
        any_expand = any_expand || expands(h);
        ++n;
    }
    const int space = available - (n > 1 ? (n - 1) * spacing : 0);
    if (!any_expand) {
        if (sum > space) warn_overflow(b, s);
        return sum;
    }
    const int total = box_distribute_each(
        [&](auto&& fn) {
            for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
                // the item lives in the node between passes: size in w/h, `fixed` in the scratch flag
                UiHot& h = b.hot[c];
                std::int16_t& size = horizontal ? h.w : h.h;
                BoxItem it{.min = horizontal ? h.min_w : h.min_h, .stretch = h.stretch, .expand = expands(h),
                           .fixed = (h.flags & kUiScratch) != 0, .size = size};
                fn(it);
                size = i16(it.size);
                h.flags = static_cast<std::uint8_t>(it.fixed ? h.flags | kUiScratch : h.flags & ~kUiScratch);
            }
        },
        space);
    if (total > space) warn_overflow(b, s);
    return total;
}

/// One axis of the children of `s` (content start `start`, size `size`).
void arrange_axis(UiBlock& b, int s, bool horizontal, int start, int size) {
    const int axis = box_axis(b, s);
    const int spacing = box_spacing(b, s);
    const bool along = (axis == 1 && !horizontal) || (axis == 2 && horizontal);
    if (along) {
        distribute(b, s, horizontal, size);
        int at = start;
        for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
            (horizontal ? b.hot[c].x : b.hot[c].y) = i16(at);
            at += (horizontal ? b.hot[c].w : b.hot[c].h) + spacing;
        }
        return;
    }
    if (axis == 3) {
        // track sizes in one walk (rows past the table are found by a walk), then each child by running counters:
        // no division per child
        std::int16_t sizes[kGridTable], starts[kGridTable];
        const int cols = grid_columns(b, s);
        const int needed = grid_tracks(b, s, horizontal, sizes);
        int at = start;
        for (int k = 0; k < kGridTable && k < needed; ++k) {
            starts[k] = i16(at);
            at += sizes[k] + spacing;
        }
        int col = 0, row = 0;
        for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
            const int k = horizontal ? col : row;
            int pos = 0, track = 0;
            if (k < kGridTable) {
                pos = starts[k];
                track = sizes[k];
            } else {
                pos = at;
                for (int j = kGridTable; j < k; ++j) pos += grid_track_at(b, s, horizontal, j) + spacing;
                track = grid_track_at(b, s, horizontal, k);
            }
            UiHot& h = b.hot[c];
            const AxisSpan span = box_align(align_of(h), track, horizontal ? h.min_w : h.min_h);
            (horizontal ? h.x : h.y) = i16(pos + span.pos);
            (horizontal ? h.w : h.h) = i16(span.size);
            if (++col == cols) {
                col = 0;
                ++row;
            }
        }
        return;
    }
    for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
        UiHot& h = b.hot[c];
        if (axis == 0) { // anchored (roots, stacks)
            const UiRect parent{horizontal ? start : 0, horizontal ? 0 : start, horizontal ? size : 0,
                                horizontal ? 0 : size};
            const UiRect r = anchor_place(parent, h.min_w, h.min_h, place_of(b.cold[c]));
            (horizontal ? h.x : h.y) = i16(horizontal ? r.x : r.y);
            (horizontal ? h.w : h.h) = i16(horizontal ? r.w : r.h);
        } else { // across a box, or a panel's one child (fills)
            const AxisSpan span = box_align(axis == 4 ? UiAlign::Fill : align_of(h), size, horizontal ? h.min_w : h.min_h);
            (horizontal ? h.x : h.y) = i16(start + span.pos);
            (horizontal ? h.w : h.h) = i16(span.size);
        }
    }
}

/// A scroll box's children stacked from the top, moved up by the scroll, with their window flags.
void arrange_scroll_box(UiBlock& b, int s, int start, int size) {
    const int spacing = box_spacing(b, s);
    UiBoxData& x = b.cold[s].data.box;
    int at = 0, i = 0, scroll = 0;
    for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c), ++i) {
        if (i == x.first) scroll = at;
        b.hot[c].h = b.hot[c].min_h;
        at += b.hot[c].h + spacing;
    }
    if (x.first >= i) x.first = static_cast<std::uint16_t>(i > 0 ? i - 1 : 0);
    x.scroll_px = i16(scroll);
    at = start - scroll;
    for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
        b.hot[c].y = i16(at);
        at += b.hot[c].h + spacing;
    }
    (void)size;
}

/// The window flag of `c` from its parent's: inside a scroll box's content area, whole.
void window_flag(UiBlock& b, int c) {
    UiHot& h = b.hot[c];
    const int p = h.parent;
    bool in = p == kUiNone || (b.hot[p].flags & kUiInWindow) != 0;
    if (in && p != kUiNone && kind_of(b, p) == UiKind::ScrollBox) {
        const Pad pad = padding_of(b, p);
        const int top = b.hot[p].y + pad.t, bottom = b.hot[p].y + b.hot[p].h - pad.b;
        in = h.y >= top && h.y + h.h <= bottom;
    }
    h.flags = static_cast<std::uint8_t>(in ? h.flags | kUiInWindow : h.flags & ~kUiInWindow);
}

} // namespace

void arrange_widths(UiBlock& b, int root) {
    for (int s = root; s >= 0; s = preorder_next_visible(b, root, s)) {
        ++b.layout_visits;
        if (!is_container(kind_of(b, s)) || b.hot[s].first_child == kUiNone) continue;
        const Pad p = padding_of(b, s);
        arrange_axis(b, s, true, b.hot[s].x + p.l, b.hot[s].w - p.l - p.r);
    }
}

void arrange_heights(UiBlock& b, int root) {
    int order = 0;
    for (int s = root; s >= 0; s = preorder_next_visible(b, root, s)) {
        ++b.layout_visits;
        b.hot[s].order = static_cast<std::uint16_t>(order++);
        window_flag(b, s);
        if (!is_container(kind_of(b, s)) || b.hot[s].first_child == kUiNone) continue;
        const Pad p = padding_of(b, s);
        const int start = b.hot[s].y + p.t, size = b.hot[s].h - p.t - p.b;
        if (kind_of(b, s) == UiKind::ScrollBox) {
            arrange_scroll_box(b, s, start, size);
        } else {
            arrange_axis(b, s, false, start, size);
        }
    }
}

void layout_scroll_window(UiBlock& b, int s) {
    const UiBoxData& x = b.cold[s].data.box;
    const int before = x.scroll_px;
    const Pad p = padding_of(b, s);
    arrange_scroll_box(b, s, b.hot[s].y + p.t, b.hot[s].h - p.t - p.b);
    const int delta = b.cold[s].data.box.scroll_px - before;
    // translate the grandchildren (the children were placed) and refresh the window flags; no measuring
    for (int c = first_visible_child(b, s); c >= 0; c = next_visible_sibling(b, c)) {
        window_flag(b, c);
        for (int d = preorder_next_visible(b, c, c); d >= 0; d = preorder_next_visible(b, c, d)) {
            b.hot[d].y = i16(b.hot[d].y - delta);
            window_flag(b, d);
        }
    }
}

} // namespace nnh::detail
