// Queries (spec 028 FR-13, FR-30): kinds, keys, rectangles (a pending layout completed first), screens, visibility,
// state, text, values, list information. A stale widget answers zero/none (and asserts in Debug/Development).

#include "ui/ui_block.hpp"

namespace nnh {
namespace {

int live(const detail::UiBlock* b, UiWidget w) {
    if (b == nullptr) return -1;
    const int s = detail::slot_of(*b, w);
    if (s < 0 && w) NNH_UI_MISUSE(*const_cast<detail::UiBlock*>(b), "Ui: a query with a stale widget");
    return s;
}

} // namespace

namespace detail {

bool enabled_chain(const UiBlock& b, int s) {
    for (int n = s; n != kUiNone; n = b.hot[n].parent) {
        if ((b.hot[n].flags & kUiEnabled) == 0) return false;
    }
    return true;
}

UiState state_of(const UiBlock& b, int s) {
    const UiHot& n = b.hot[s];
    if (!enabled_chain(b, s)) return UiState::Disabled;
    if ((b.pressed_slot == s && b.pressed_frames > 0) || (n.flags & kUiTouched) != 0) return UiState::Pressed;
    if (b.focus == s) return UiState::Focused;
    if (kind_of(b, s) == UiKind::Toggle && b.cold[s].data.toggle.checked != 0) return UiState::Checked;
    return UiState::Normal;
}

int value_of(const UiBlock& b, int s) {
    const UiKindData& d = b.cold[s].data;
    switch (kind_of(b, s)) {
    case UiKind::Slider: return d.slider.value;
    case UiKind::Progress: return d.progress.value;
    case UiKind::Toggle: return d.toggle.checked;
    case UiKind::Choice: return d.choice.index;
    case UiKind::List:
    case UiKind::Grid: return d.list.count > 0 ? d.list.focused : -1;
    default: return 0;
    }
}

} // namespace detail

bool Ui::alive(UiWidget w) const { return block_ != nullptr && detail::slot_of(*block_, w) >= 0; }

UiKind Ui::kind(UiWidget w) const {
    const int s = live(block_, w);
    return s < 0 ? UiKind::Root : detail::kind_of(*block_, s);
}

UiKey Ui::key(UiWidget w) const {
    const int s = live(block_, w);
    return s < 0 ? UiKey{} : UiKey(static_cast<int>(block_->hot[s].key));
}

UiRect Ui::rect(UiWidget w) const {
    const int s = live(block_, w);
    if (s < 0) return {};
    detail::layout_if_dirty(*block_);
    return detail::rect_of(block_->hot[s]);
}

Screen Ui::screen(UiWidget w) const {
    const int s = live(block_, w);
    return s >= 0 && detail::root_index_of(*block_, s) == 1 ? Screen::Bottom : Screen::Top;
}

bool Ui::visible(UiWidget w) const {
    const int s = live(block_, w);
    return s >= 0 && detail::visible_chain(*block_, s);
}

bool Ui::enabled(UiWidget w) const {
    const int s = live(block_, w);
    return s >= 0 && detail::enabled_chain(*block_, s);
}

UiState Ui::state(UiWidget w) const {
    const int s = live(block_, w);
    return s < 0 ? UiState::Normal : detail::state_of(*block_, s);
}

const char* Ui::text(UiWidget w) const {
    const int s = live(block_, w);
    return s < 0 ? "" : detail::text_of(*block_, s);
}

int Ui::value(UiWidget w) const {
    const int s = live(block_, w);
    return s < 0 ? 0 : detail::value_of(*block_, s);
}

UiListInfo Ui::list_info(UiWidget w) const {
    const int s = live(block_, w);
    if (s < 0 || !detail::is_list(detail::kind_of(*block_, s))) return {};
    const detail::UiListData& l = block_->cold[s].data.list;
    UiListInfo info;
    info.count = l.count;
    info.focused = l.count > 0 ? l.focused : -1;
    info.first_visible = l.first;
    info.visible_rows = l.rows;
    return info;
}

UiWidget Ui::focused() const {
    return block_ == nullptr || block_->focus == detail::kUiNone ? UiWidget{}
                                                                   : detail::handle_of(*block_, block_->focus);
}

UiKey Ui::focused_key() const {
    return block_ == nullptr || block_->focus == detail::kUiNone ? UiKey{}
                                                                   : UiKey(static_cast<int>(block_->hot[block_->focus].key));
}

int Ui::modal_depth() const { return block_ == nullptr ? 0 : block_->modal_depth; }

} // namespace nnh
