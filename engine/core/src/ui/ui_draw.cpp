// Drawing (spec 028 FR-28, FR-7, FR-18, FR-27): per visible root (top, then bottom), pre-order over visible nodes —
// parents before children — with render offsets and opacity accumulated down the tree (integers); a scroll box shows
// only its window's children; an open modal's subtree is skipped in place and drawn after the rest of its root; then
// the focus cursor. draw() completes a pending layout first and changes nothing else.

#include "nnh/profile.hpp"
#include "ui/ui_cursor.hpp"
#include "ui/ui_draw.hpp"
#include "ui/ui_draw_out.hpp"

namespace nnh::detail {
namespace {

/// The next node in pre-order that isn't inside `s`'s subtree.
int skip_subtree(const UiBlock& b, int root, int s) {
    while (s != root) {
        int n = b.hot[s].next;
        while (n != kUiNone && (b.hot[n].flags & kUiVisible) == 0) n = b.hot[n].next;
        if (n != kUiNone) return n;
        s = b.hot[s].parent;
    }
    return -1;
}

bool is_open_modal(const UiBlock& b, int s) {
    for (int i = 0; i < b.modal_depth; ++i) {
        if (b.modals[i].container == s) return true;
    }
    return false;
}

/// Draws the subtree of `top` (its open modals other than `top` itself skipped).
void draw_subtree(const UiBlock& b, int top) {
    for (int s = top; s >= 0;) {
        if (s != top && ((b.hot[s].flags & kUiInWindow) == 0 || is_open_modal(b, s))) {
            s = skip_subtree(b, top, s);
            continue;
        }
        if (has_look(kind_of(b, s))) draw_widget(b, context_of(b, s), s); // no context for a box
        s = preorder_next_visible(b, top, s);
    }
}

void draw_cursor(const UiBlock& b, int root_index) {
    const int f = b.focus;
    const UiCursorStyle& st = b.theme->cursor;
    if (f == kUiNone || root_index_of(b, f) != root_index || b.cursor.hidden || !b.cursor.placed) return;
    if (!st.outline && st.image.texture < 0) return;
    DrawCtx d = context_of(b, f);
    const CursorPulse pulse = cursor_pulse(b);
    d.opacity = (d.opacity * pulse.opacity + 127) / 255;
    if (st.outline) out_outline(b, d, cursor_target_rect(b, f), st.outline_color);
    if (st.image.texture >= 0) {
        const Region& r = st.image.region;
        const int m = 8 - static_cast<int>(st.anchor); // the image's opposite point meets the widget's
        const int mx = m % 3, my = m / 3;
        const int ix = b.cursor.x - (mx == 0 ? 0 : (mx == 1 ? r.w >> 1 : r.w)) + st.x + pulse.dx;
        const int iy = b.cursor.y - (my == 0 ? 0 : (my == 1 ? r.h >> 1 : r.h)) + st.y;
        out_sprite(b, d, st.image.texture, r, ix, iy, 1.0f, 1.0f, colors::white);
    }
}

} // namespace

DrawCtx context_of(const UiBlock& b, int s) {
    DrawCtx d;
    d.screen = root_index_of(b, s) == 1 ? Screen::Bottom : Screen::Top;
    d.layer = b.layers[root_index_of(b, s)];
    for (int n = s; n != kUiNone; n = b.hot[n].parent) {
        const UiCold& c = b.cold[n];
        d.dx += c.offset_x;
        d.dy += c.offset_y;
        if (c.opacity != 255) d.opacity = (d.opacity * c.opacity + 127) / 255; // (o × 255 + 127) / 255 = o
    }
    return d;
}

void draw_root(const UiBlock& b, int root_index) {
    const int root = b.roots[root_index];
    if (root == kUiNone || !b.root_visible[root_index]) return;
    draw_subtree(b, root);
    for (int i = 0; i < b.modal_depth; ++i) {
        const int m = b.modals[i].container;
        if (root_index_of(b, m) == root_index && visible_chain(b, m)) draw_subtree(b, m);
    }
    draw_cursor(b, root_index);
}

} // namespace nnh::detail

namespace nnh {

void Ui::draw() const {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    NNH_PROFILE_SCOPE("ui draw");
    detail::layout_if_dirty(*b);
    b->draws = 0;
    b->draw_hash = 2166136261u;
    for (int i = 0; i < 2; ++i) detail::draw_root(*b, i);
}

} // namespace nnh
