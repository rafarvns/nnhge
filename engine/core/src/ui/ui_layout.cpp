// The layout driver (spec 028 FR-13, D48): a root marked dirty is laid out by four linear walks — minimum widths
// bottom-up, widths top-down, wrapped heights bottom-up, heights and positions top-down — at the end of update(), on a
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

int box_axis(const UiBlock& b, int s) {
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

namespace {

int deepest_visible(const UiBlock& b, int s) {
    for (int c = first_visible_child(b, s); c >= 0; c = first_visible_child(b, c)) s = c;
    return s;
}

} // namespace

int postorder_first(const UiBlock& b, int root) { return deepest_visible(b, root); }

int postorder_next(const UiBlock& b, int root, int s) {
    if (s == root) return -1;
    const int sibling = next_visible_sibling(b, s);
    return sibling >= 0 ? deepest_visible(b, sibling) : b.hot[s].parent;
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
    measure_widths(b, root);
    arrange_widths(b, root);
    measure_heights(b, root);
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
