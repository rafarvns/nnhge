// Focus (spec 028 FR-14, FR-15, FR-17): what can take focus, entering containers (their remembered child if still a
// target, else their default, else their first target in tree order), setting focus (remembered by every ancestor),
// and repairing it after a hide, disable or destroy — never a dangling focus.

#include "ui/ui_block.hpp"
#include "ui/ui_focus.hpp"

namespace nnh::detail {
namespace {

constexpr int kMaxEnterDepth = 64;

bool under(const UiBlock& b, int s, int ancestor) {
    for (int n = s; n != kUiNone; n = b.hot[n].parent) {
        if (n == ancestor) return true;
    }
    return false;
}

int enter(const UiBlock& b, int s, int depth) {
    if (is_target(b, s)) return s;
    if (depth > kMaxEnterDepth || !is_container(kind_of(b, s)) || !visible_chain(b, s) || !in_modal_scope(b, s)) {
        return -1;
    }
    const UiBoxData& x = b.cold[s].data.box;
    for (const std::uint16_t pref : {x.last, x.def}) {
        if (pref == kUiNone || !under(b, pref, s) || pref == s) continue;
        const int t = enter(b, pref, depth + 1);
        if (t >= 0) return t;
    }
    for (int n = preorder_next_visible(b, s, s); n >= 0; n = preorder_next_visible(b, s, n)) {
        if (is_target(b, n)) return n;
    }
    return -1;
}

} // namespace

bool inert_allowed(const UiBlock& b, int s) {
    for (int n = b.hot[s].parent; n != kUiNone; n = b.hot[n].parent) {
        if (is_container(kind_of(b, n)) && kind_of(b, n) != UiKind::Root && (b.cold[n].data.box.bits & 4) != 0) {
            return true;
        }
    }
    return false;
}

bool inert(const UiBlock& b, int s) { return !enabled_chain(b, s) && inert_allowed(b, s); }

bool in_modal_scope(const UiBlock& b, int s) {
    return b.modal_depth == 0 || under(b, s, b.modals[b.modal_depth - 1].container);
}

bool is_target(const UiBlock& b, int s, bool) {
    if (s < 0 || s >= b.capacity || b.hot[s].kind == kUiFreeKind) return false;
    const UiKind k = kind_of(b, s);
    if (!is_interactive(k)) return false;
    if (is_list(k) && b.cold[s].data.list.count == 0) return false;
    if (!b.ignore_window && (b.hot[s].flags & kUiInWindow) == 0) return false;
    if (!visible_chain(b, s) || !in_modal_scope(b, s)) return false;
    return enabled_chain(b, s) || inert_allowed(b, s);
}

int enter_target(const UiBlock& b, int s) { return s < 0 ? -1 : enter(b, s, 0); }

void set_focus(UiBlock& b, int s, bool report) {
    const int old = b.focus == kUiNone ? -1 : b.focus;
    if (s == old) return;
    b.focus = s < 0 ? kUiNone : static_cast<std::uint16_t>(s);
    if (s >= 0) {
        for (int child = s, p = b.hot[s].parent; p != kUiNone; child = p, p = b.hot[p].parent) {
            if (is_container(kind_of(b, p))) b.cold[p].data.box.last = static_cast<std::uint16_t>(child);
        }
        if (b.input.hide_cursor_after_touch && !b.touch.active) b.cursor.hidden = false;
    }
    if (report) {
        add_result(b, UiResultKind::FocusChanged, s < 0 ? -1 : s, 0, s >= 0 && is_list(kind_of(b, s))
                                                                         ? b.cold[s].data.list.focused
                                                                         : -1,
                   old);
    }
}

RepairPoint repair_point(const UiBlock& b, int s) {
    RepairPoint p;
    p.parent = b.hot[s].parent == kUiNone ? -1 : b.hot[s].parent;
    p.next = b.hot[s].next == kUiNone ? -1 : b.hot[s].next;
    p.prev = p.parent >= 0 && b.hot[p.parent].first_child != s ? b.hot[s].prev : -1;
    p.root = b.roots[root_index_of(b, s)];
    return p;
}

int repair_target(const UiBlock& b, const RepairPoint& p) {
    for (int n = p.next; n >= 0 && n != kUiNone; n = b.hot[n].next == kUiNone ? -1 : b.hot[n].next) {
        const int t = enter_target(b, n);
        if (t >= 0) return t;
    }
    if (p.parent >= 0) {
        for (int n = p.prev; n >= 0; n = n == b.hot[p.parent].first_child ? -1 : b.hot[n].prev) {
            const int t = enter_target(b, n);
            if (t >= 0) return t;
        }
    }
    const int root = p.root;
    if (root == kUiNone) return -1;
    const int def = b.cold[root].data.box.def;
    if (def != kUiNone) {
        const int t = enter_target(b, def);
        if (t >= 0) return t;
    }
    return -1;
}

} // namespace nnh::detail

namespace nnh {

void Ui::focus(UiWidget w) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    const int s = detail::slot_of(*b, w);
    if (s < 0) {
        if (w) NNH_UI_MISUSE(*b, "Ui::focus: a stale widget");
        if (!w) detail::set_focus(*b, -1, false);
        return;
    }
    detail::layout_if_dirty(*b); // window flags
    const int t = detail::enter_target(*b, s);
    if (t >= 0) detail::set_focus(*b, t, false);
}

void Ui::focus(UiKey k) {
    if (block_ == nullptr) return;
    const UiWidget w = find(k);
    if (w) focus(w);
}

} // namespace nnh
