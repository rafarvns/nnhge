// Lists and grids (spec 028 FR-11, FR-15): a focused item moved by directions (grids by columns), whole-row scrolling
// with a margin of rows kept visible, pages, wrap on a fresh press, disabled rows skipped (or refused when inert), and
// the row pool: item i lives in pooled row first_row + i % (rows × columns), filled by the game's function only when
// it comes into view or is marked changed — a list of 10 000 items costs what one of 20 does.

#include "ui/ui_focus.hpp"
#include "ui/ui_layout.hpp"

namespace nnh::detail {
namespace {

int cells_of(const UiListData& l) { return l.rows * l.columns; }

/// Rows of items in total.
int total_rows(const UiListData& l) { return (l.count + l.columns - 1) / l.columns; }

int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// Scrolls (in whole rows) so that `item` is visible with the margin.
void keep_visible(UiListData& l, int item) {
    const int row = item / l.columns;
    const int margin = clamp(l.margin, 0, (l.rows - 1) / 2);
    int first = l.first / l.columns;
    if (row < first + margin) first = row - margin;
    if (row > first + l.rows - 1 - margin) first = row - (l.rows - 1 - margin);
    first = clamp(first, 0, total_rows(l) > l.rows ? total_rows(l) - l.rows : 0);
    l.first = static_cast<std::uint16_t>(first * l.columns);
}

UiRowSlot& pooled(UiBlock& b, const UiListData& l, int item) { return b.rows[l.first_row + item % cells_of(l)]; }

void fill_one(UiBlock& b, int s, int item) {
    const UiListData& l = b.cold[s].data.list;
    UiRowSlot& slot = pooled(b, l, item);
    if (slot.index == item) return;
    slot.row = UiRow{};
    slot.index = item;
    if (l.fill != kUiNoSlot) fn_at<UiRowFill>(b, l.fill)(item, slot.row);
}

/// A row the navigation may land on: enabled, or disabled where disabled widgets are inert (FR-15).
bool row_usable(UiBlock& b, int s, int item) { return list_row_enabled(b, s, item) || inert_allowed(b, s); }

/// Moves the focused item to `item` (scrolling), reporting the move.
void focus_item(UiBlock& b, int s, int item) {
    UiListData& l = b.cold[s].data.list;
    const int old = l.focused;
    l.focused = static_cast<std::uint16_t>(item);
    keep_visible(l, item);
    if (old != item) add_result(b, UiResultKind::FocusChanged, s, 0, item, s);
}

} // namespace

int list_row_height(const UiBlock& b, int s) {
    const int h = b.cold[s].data.list.row_height > 0 ? b.cold[s].data.list.row_height : style_of(b, s).list.row_height;
    return h < 1 ? 1 : h;
}

int list_visible_rows(const UiBlock& b, int s) { return b.cold[s].data.list.rows; }

/// The rows' area: the content rectangle without the scroll bar.
UiRect list_area(const UiBlock& b, int s) {
    const Pad p = padding_of(b, s);
    const UiHot& n = b.hot[s];
    return {n.x + p.l, n.y + p.t, n.w - p.l - p.r - style_of(b, s).list.scroll_width, n.h - p.t - p.b};
}

UiRect list_item_rect(const UiBlock& b, int s, int item) {
    const UiListData& l = b.cold[s].data.list;
    if (item < l.first || item >= l.first + cells_of(l) || item >= l.count) return {};
    const UiRect a = list_area(b, s);
    const int cell_w = a.w / l.columns, row_h = list_row_height(b, s);
    const int k = item - l.first;
    return {a.x + (k % l.columns) * cell_w, a.y + (k / l.columns) * row_h, cell_w, row_h};
}

int list_item_at(const UiBlock& b, int s, int x, int y) {
    const UiListData& l = b.cold[s].data.list;
    const UiRect a = list_area(b, s);
    if (!a.contains(x, y) || l.columns < 1) return -1;
    const int cell_w = a.w / l.columns, row_h = list_row_height(b, s);
    if (cell_w < 1) return -1;
    const int row = (y - a.y) / row_h, col = (x - a.x) / cell_w;
    if (row >= l.rows || col >= l.columns) return -1;
    const int item = l.first + row * l.columns + col;
    return item < l.count ? item : -1;
}

void list_fill_rows(UiBlock& b, int s) {
    const UiListData& l = b.cold[s].data.list;
    const int end = l.first + cells_of(l) < l.count ? l.first + cells_of(l) : l.count;
    for (int i = l.first; i < end; ++i) fill_one(b, s, i);
}

bool list_row_enabled(UiBlock& b, int s, int item) {
    const UiListData& l = b.cold[s].data.list;
    if (item < 0 || item >= l.count) return false;
    if (item >= l.first && item < l.first + cells_of(l)) {
        fill_one(b, s, item);
        return pooled(b, l, item).row.enabled;
    }
    UiRow row; // off screen (rare: the moves below scroll first)
    if (l.fill != kUiNoSlot) fn_at<UiRowFill>(b, l.fill)(item, row);
    return row.enabled;
}

bool list_direction(UiBlock& b, int s, UiDir d, bool fresh) {
    UiListData& l = b.cold[s].data.list;
    if (l.count == 0) return false;
    const bool vertical = d == UiDir::Up || d == UiDir::Down;
    if (!vertical && l.columns == 1) return false; // a list passes left/right on
    const int forward = d == UiDir::Down || d == UiDir::Right ? 1 : -1;
    const int step = vertical ? forward * l.columns : forward;
    const bool wraps = (l.bits & 1) != 0 && fresh;
    int item = l.focused;
    for (int tries = 0; tries < l.count; ++tries) {
        int next = item + step;
        if (!vertical && next / l.columns != item / l.columns) next = -1; // left/right stay in the row
        if (next < 0 || next >= l.count) {
            if (!wraps || !vertical) return false;
            const int col = l.focused % l.columns;
            next = forward > 0 ? col : col + ((l.count - 1 - col) / l.columns) * l.columns;
            if (next >= l.count) return false;
        }
        if (next == l.focused) return false;
        item = next;
        const std::uint16_t first = l.first;
        keep_visible(l, item); // rows come into view, then the row is checked
        if (row_usable(b, s, item)) {
            l.first = first;
            focus_item(b, s, item);
            return true;
        }
        l.first = first;
    }
    return false;
}

void list_page(UiBlock& b, int s, int pages) {
    UiListData& l = b.cold[s].data.list;
    if (l.count == 0) return;
    const int page = cells_of(l);
    const int target = clamp(l.focused + pages * page, 0, l.count - 1);
    const int first_row = clamp(l.first / l.columns + pages * l.rows, 0,
                                total_rows(l) > l.rows ? total_rows(l) - l.rows : 0);
    l.first = static_cast<std::uint16_t>(first_row * l.columns);
    int item = target;
    while (item >= 0 && item < l.count && !row_usable(b, s, item)) item -= pages; // back toward the start
    if (item < 0 || item >= l.count) item = l.focused;
    // the page stays where it was put; the margin rule only applies when the item fell outside it
    const int old = l.focused;
    l.focused = static_cast<std::uint16_t>(item);
    if (item < l.first || item >= l.first + cells_of(l)) keep_visible(l, item);
    if (old != item) add_result(b, UiResultKind::FocusChanged, s, 0, item, s);
}

void list_set_item(UiBlock& b, int s, int item) {
    UiListData& l = b.cold[s].data.list;
    if (l.count == 0) return;
    l.focused = static_cast<std::uint16_t>(clamp(item, 0, l.count - 1));
    keep_visible(l, l.focused);
}

void list_set_scroll(UiBlock& b, int s, int first) {
    UiListData& l = b.cold[s].data.list;
    const int row = clamp(first < 0 ? 0 : first / l.columns, 0, total_rows(l) > l.rows ? total_rows(l) - l.rows : 0);
    l.first = static_cast<std::uint16_t>(row * l.columns);
}

} // namespace nnh::detail

namespace nnh {
namespace {

int list_slot(detail::UiBlock* b, UiWidget w, [[maybe_unused]] const char* who) {
    if (b == nullptr) return -1;
    const int s = detail::slot_of(*b, w);
    if (s < 0 || !detail::is_list(detail::kind_of(*b, s))) {
        NNH_UI_MISUSE(*b, who);
        return -1;
    }
    return s;
}

} // namespace

void Ui::set_item_count(UiWidget w, int count) {
    const int s = list_slot(block_, w, "Ui::set_item_count: a list or grid");
    if (s < 0) return;
    detail::UiListData& l = block_->cold[s].data.list;
    const int c = count < 0 ? 0 : (count > kMaxUiItems ? kMaxUiItems : count);
    const bool was_target = detail::is_target(*block_, s);
    const detail::RepairPoint at = detail::repair_point(*block_, s);
    l.count = static_cast<std::uint16_t>(c);
    for (int i = 0; i < l.rows * l.columns; ++i) {
        detail::UiRowSlot& r = block_->rows[l.first_row + i];
        if (r.index >= c) r.index = -1;
    }
    if (l.focused >= c) l.focused = static_cast<std::uint16_t>(c > 0 ? c - 1 : 0);
    detail::list_set_scroll(*block_, s, l.first);
    if (c > 0) detail::list_set_item(*block_, s, l.focused);
    if (was_target && c == 0 && block_->focus == s) detail::set_focus(*block_, detail::repair_target(*block_, at), false);
}

void Ui::mark_items_changed(UiWidget w, int first, int count) {
    const int s = list_slot(block_, w, "Ui::mark_items_changed: a list or grid");
    if (s < 0) return;
    const detail::UiListData& l = block_->cold[s].data.list;
    const int end = count < 0 ? kMaxUiItems + 1 : first + count;
    for (int i = 0; i < l.rows * l.columns; ++i) {
        detail::UiRowSlot& r = block_->rows[l.first_row + i];
        if (r.index >= first && r.index < end) r.index = -1;
    }
}

void Ui::set_item(UiWidget w, int index) {
    const int s = list_slot(block_, w, "Ui::set_item: a list or grid");
    if (s >= 0) detail::list_set_item(*block_, s, index);
}

void Ui::set_scroll(UiWidget w, int first) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    const int s = detail::slot_of(*b, w);
    if (s >= 0 && detail::is_list(detail::kind_of(*b, s))) {
        detail::list_set_scroll(*b, s, first);
    } else if (s >= 0 && detail::kind_of(*b, s) == UiKind::ScrollBox) {
        detail::layout_if_dirty(*b);
        detail::scroll_set_first(*b, s, first);
    } else {
        NNH_UI_MISUSE(*b, "Ui::set_scroll: a list, grid or scroll box");
    }
}

} // namespace nnh
