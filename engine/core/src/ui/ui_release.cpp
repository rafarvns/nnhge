// Slices of the extra text table (choice options) and of the row pool (lists, grids), taken contiguously and
// compacted when released (the owners' first indexes rewritten), and everything a destroyed widget held.

#include <cstring>

#include "ui/ui_block.hpp"

namespace nnh::detail {

int texts_take(UiBlock& b, int count) {
    if (count < 0 || b.texts_used + count > b.texts_capacity) return -1;
    const int first = b.texts_used;
    b.texts_used += count;
    return first;
}

int rows_take(UiBlock& b, int count) {
    if (count < 0 || b.rows_used + count > b.rows_capacity) return -1;
    const int first = b.rows_used;
    for (int i = 0; i < count; ++i) b.rows[first + i] = UiRowSlot{};
    b.rows_used += count;
    return first;
}

namespace {

void texts_release(UiBlock& b, int first, int count) {
    for (int i = first; i < first + count; ++i) release_text(b, b.text_kinds[i], b.texts[i]);
    for (int i = first + count; i < b.texts_used; ++i) {
        b.texts[i - count] = b.texts[i];
        b.text_kinds[i - count] = b.text_kinds[i];
    }
    b.texts_used -= count;
    for (int s = 0; s < b.capacity; ++s) {
        if (kind_of(b, s) == UiKind::Choice && b.hot[s].kind != kUiFreeKind &&
            b.cold[s].data.choice.first_text > first) {
            b.cold[s].data.choice.first_text = static_cast<std::uint16_t>(b.cold[s].data.choice.first_text - count);
        }
    }
}

void rows_release(UiBlock& b, int first, int count) {
    for (int i = first + count; i < b.rows_used; ++i) b.rows[i - count] = b.rows[i];
    b.rows_used -= count;
    for (int s = 0; s < b.capacity; ++s) {
        if (b.hot[s].kind == kUiFreeKind || !is_list(kind_of(b, s))) continue;
        UiListData& l = b.cold[s].data.list;
        if (l.first_row > first) l.first_row = static_cast<std::uint16_t>(l.first_row - count);
    }
}

} // namespace

void release_node(UiBlock& b, int s) {
    UiCold& c = b.cold[s];
    release_text(b, c.text_kind, c.text);
    c.text_kind = kUiTextNone;
    fn_release(b, c.on_activate);
    fn_release(b, c.on_change);
    switch (kind_of(b, s)) {
    case UiKind::Choice:
        texts_release(b, c.data.choice.first_text, c.data.choice.count);
        break;
    case UiKind::List:
    case UiKind::Grid:
        fn_release(b, c.data.list.fill);
        rows_release(b, c.data.list.first_row, c.data.list.rows * c.data.list.columns);
        break;
    case UiKind::Slider: fn_release(b, c.data.slider.format); break;
    case UiKind::Progress: fn_release(b, c.data.progress.format); break;
    case UiKind::Label:
        reveal_forget(b, s);
        if ((c.data.label.bits & 1) != 0) --b.wrap_labels[root_index_of(b, s)];
        break;
    default: break;
    }
    if (b.focus == s) b.focus = kUiNone; // repaired by the caller
}

} // namespace nnh::detail
