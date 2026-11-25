// Touch on the bottom screen (spec 028 FR-19, FR-18): a touch that starts on a focusable widget focuses it and shows
// its pressed look; releasing inside activates it (a button activates, a toggle flips, a choice arrow steps, a list
// row is selected and activated); sliding out cancels; a slider follows x; a list, grid or scroll box dragged past
// kUiDragThreshold pixels scrolls by whole rows (toward zero) and cancels the press. Only the bottom root reacts; an
// open modal takes every touch (outside it: ignored, or cancel with outside_touch_cancels).

#include "ui/ui_focus.hpp"
#include "ui/ui_layout.hpp"
#include "ui/ui_parts.hpp"

namespace nnh::detail {
namespace {

void set_touched(UiBlock& b, bool on) {
    const int w = b.touch.widget;
    if (w == kUiNone) return;
    b.hot[w].flags = static_cast<std::uint8_t>(on ? b.hot[w].flags | kUiTouched : b.hot[w].flags & ~kUiTouched);
}

/// The last visible node containing the point (later siblings and children draw on top), -1 when none.
int hit(const UiBlock& b, int root, int x, int y) {
    int found = -1;
    for (int n = root; n >= 0; n = preorder_next_visible(b, root, n)) {
        const UiHot& h = b.hot[n];
        if ((h.flags & kUiInWindow) != 0 && rect_of(h).contains(x, y)) found = n;
    }
    return found;
}

int up_to(const UiBlock& b, int n, bool (*want)(UiKind)) {
    for (; n != kUiNone; n = b.hot[n].parent) {
        if (want(kind_of(b, n))) return n;
    }
    return -1;
}

bool scrolls(UiKind k) { return is_list(k) || k == UiKind::ScrollBox; }

int scroll_step(const UiBlock& b, int sc) {
    if (is_list(kind_of(b, sc))) return list_row_height(b, sc);
    const int first = first_visible_child(b, sc);
    const int h = first >= 0 ? b.hot[first].h + box_spacing(b, sc) : 1;
    return h < 1 ? 1 : h;
}

void start(UiBlock& b, int x, int y) {
    touch_reset(b);
    UiTouch& t = b.touch;
    t.active = true;
    t.start_x = static_cast<std::int16_t>(x);
    t.start_y = static_cast<std::int16_t>(y);
    const int root = b.roots[1];
    if (root == kUiNone || !b.root_visible[1]) return;
    int scope = root;
    if (b.modal_depth > 0) {
        const UiModal& m = b.modals[b.modal_depth - 1];
        if (root_index_of(b, m.container) != 1 || !rect_of(b.hot[m.container]).contains(x, y)) {
            if (m.outside_touch_cancels) {
                add_result(b, UiResultKind::Cancelled, b.focus == kUiNone ? m.container : b.focus);
                modal_close(b);
            }
            return;
        }
        scope = m.container;
    }
    const int n = hit(b, scope, x, y);
    if (n < 0) return;
    const int sc = up_to(b, n, scrolls);
    if (sc >= 0) {
        t.scroller = static_cast<std::uint16_t>(sc);
        t.start_scroll = is_list(kind_of(b, sc)) ? b.cold[sc].data.list.first : b.cold[sc].data.box.first;
    }
    const int w = up_to(b, n, is_interactive);
    if (w < 0 || !is_target(b, w)) return;
    t.widget = static_cast<std::uint16_t>(w);
    t.was_focused = b.focus == w;
    const UiKind k = kind_of(b, w);
    if (is_list(k)) {
        t.row = list_item_at(b, w, x, y);
        t.was_focused = t.was_focused && t.row == b.cold[w].data.list.focused;
    } else if (k == UiKind::Choice) {
        const ChoiceParts p = choice_parts(b, w);
        t.part = p.left.contains(x, y) ? 1 : (p.right.contains(x, y) ? 2 : 0);
    } else if (k == UiKind::Slider && !inert(b, w)) {
        const SliderParts p = slider_parts(b, w);
        if (p.track.contains(x, y)) {
            t.part = 3;
            value_set(b, w, slider_value_at(b, w, p.track, x), true);
        }
    }
    set_touched(b, true);
    set_focus(b, w, true);
    if (b.input.hide_cursor_after_touch) b.cursor.hidden = true;
}

void held(UiBlock& b, int x, int y) {
    UiTouch& t = b.touch;
    const int dy = t.start_y - y; // up is positive: the content moves on
    if (t.scroller != kUiNone && !t.scrolling && t.part != 3 && (dy > kUiDragThreshold || -dy > kUiDragThreshold)) {
        t.scrolling = true;
        set_touched(b, false);
        t.widget = kUiNone; // the press is cancelled
    }
    if (t.scrolling) {
        const int sc = t.scroller;
        const int rows = dy / scroll_step(b, sc); // toward zero
        if (is_list(kind_of(b, sc))) {
            list_set_scroll(b, sc, t.start_scroll + rows * b.cold[sc].data.list.columns);
        } else {
            scroll_set_first(b, sc, t.start_scroll + rows);
        }
        return;
    }
    if (t.widget == kUiNone) return;
    const int w = t.widget;
    if (t.part == 3) {
        value_set(b, w, slider_value_at(b, w, slider_parts(b, w).track, x), true);
        return;
    }
    set_touched(b, rect_of(b.hot[w]).contains(x, y));
}

void end(UiBlock& b, int x, int y) {
    UiTouch& t = b.touch;
    const int w = t.widget;
    if (w == kUiNone || t.scrolling || !rect_of(b.hot[w]).contains(x, y) || t.part == 3) return;
    const UiKind k = kind_of(b, w);
    if (inert(b, w)) {
        add_result(b, UiResultKind::Refused, w);
        return;
    }
    if (k == UiKind::Choice) {
        if (t.part != 0) value_direction(b, w, t.part == 1 ? UiDir::Left : UiDir::Right, true);
        return;
    }
    if (is_list(k)) {
        const int item = list_item_at(b, w, x, y);
        if (item < 0 || item != t.row) return;
        UiListData& l = b.cold[w].data.list;
        if (l.focused != item) {
            l.focused = static_cast<std::uint16_t>(item);
            add_result(b, UiResultKind::FocusChanged, w, 0, item, w);
        }
        if ((l.bits & 2) != 0 && !t.was_focused) return; // tap to select, tap again to activate
        activate(b, w, item);
        return;
    }
    if (k != UiKind::Slider) activate(b, w);
}

} // namespace

void touch_reset(UiBlock& b) {
    set_touched(b, false);
    b.touch = UiTouch{};
}

void touch_step(UiBlock& b, const UiInput& in) {
    if (in.touch_started) {
        start(b, in.touch_x, in.touch_y);
    } else if (in.touching && b.touch.active) {
        held(b, in.touch_x, in.touch_y);
    }
    if (in.touch_ended && b.touch.active) {
        if (!in.touch_started) held(b, in.touch_x, in.touch_y);
        end(b, in.touch_x, in.touch_y);
        touch_reset(b);
    }
}

} // namespace nnh::detail
