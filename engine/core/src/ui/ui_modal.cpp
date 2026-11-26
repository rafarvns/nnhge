// Modal subtrees (spec 028 FR-18): while a container is open modally, navigation, confirm/cancel and touch reach only
// its subtree, it draws after the rest of its root, and closing it gives back the focus held before it opened. Up to
// kMaxUiModalDepth nest. Opening one is an activation: controls already held are ignored until released (FR-22).

#include "ui/ui_focus.hpp"

namespace nnh {

void Ui::open_modal(UiWidget container, const UiModalOptions& o) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    const int s = detail::slot_of(*b, container);
    if (s < 0 || !detail::is_container(detail::kind_of(*b, s)) || detail::kind_of(*b, s) == UiKind::Root) {
        NNH_UI_MISUSE(*b, "Ui::open_modal: a container (not a root)");
        return;
    }
    if (b->modal_depth >= kMaxUiModalDepth) {
        NNH_UI_MISUSE(*b, "Ui::open_modal: more than 8 modals open");
        return;
    }
    detail::UiModal& m = b->modals[b->modal_depth++];
    m.container = static_cast<std::uint16_t>(s);
    m.focus_before = b->focus;
    m.focus_before_gen = b->focus == detail::kUiNone ? 0 : b->hot[b->focus].generation;
    m.cancel_closes = o.cancel_closes;
    m.outside_touch_cancels = o.outside_touch_cancels;
    b->activate_pending = true;
    detail::touch_reset(*b);
    detail::layout_if_dirty(*b);
    detail::set_focus(*b, detail::enter_target(*b, s), false);
}

namespace detail {

void modal_close(UiBlock& b0) {
    UiBlock* b = &b0;
    const detail::UiModal m = b->modals[--b->modal_depth];
    detail::touch_reset(*b);
    const int before = m.focus_before;
    const bool alive = before != detail::kUiNone && b->hot[before].kind != detail::kUiFreeKind &&
                       b->hot[before].generation == m.focus_before_gen;
    int target = alive ? detail::enter_target(*b, before) : -1;
    if (target < 0) {
        const int root = b->modal_depth > 0 ? b->modals[b->modal_depth - 1].container
                                            : b->roots[detail::root_index_of(*b, m.container)];
        target = detail::enter_target(*b, root);
    }
    detail::set_focus(*b, target, false);
}

} // namespace detail

void Ui::close_modal() {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    if (b->modal_depth == 0) {
        NNH_UI_MISUSE(*b, "Ui::close_modal: no modal is open");
        return;
    }
    detail::modal_close(*b);
}

} // namespace nnh
