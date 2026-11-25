// Drawing lists and grids (spec 028 FR-11, FR-25): the visible rows from the pool (filled first when they came into
// view), each with its row background (normal, focused while the list has the focus, disabled), icon, text cut to its
// cell and right-aligned number; a grid's cells show their icon centred (else their text); then the scroll bar from
// the first visible row and the count.

#include <cstring>

#include "ui/ui_draw.hpp"
#include "ui/ui_number.hpp"
#include "ui/ui_parts.hpp"
#include "ui/ui_text_seam.hpp"
#include "text/utf8.hpp"

namespace nnh::detail {

UiRect list_area(const UiBlock& b, int s); // ui_list.cpp

namespace {

int cut(const char* text, int cells) {
    const char* p = text;
    for (int i = 0; i < cells && *p != 0; ++i) next_codepoint(p);
    return static_cast<int>(p - text);
}

void draw_row(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st, int item, bool grid) {
    const UiListData& l = b.cold[s].data.list;
    const UiRowSlot& slot = b.rows[l.first_row + item % (l.rows * l.columns)];
    const UiRow& row = slot.row;
    const UiRect r = list_item_rect(b, s, item);
    const bool focused = item == l.focused && b.focus == s;
    const int look = !row.enabled ? 2 : (focused ? 1 : 0);
    out_background(b, d, r, st.list.row[look]);
    const UiLook& text_look = st.states[!row.enabled ? 3 : (focused ? 1 : 0)];
    const int cell = seam_cell(st.text), line = seam_line(st.text);
    const int ty = r.y + ((r.h - line) >> 1);
    if (grid) {
        if (row.icon_texture >= 0) {
            out_sprite(b, d, row.icon_texture, row.icon, r.x + ((r.w - row.icon.w) >> 1), r.y + ((r.h - row.icon.h) >> 1),
                       1, 1, text_look.tint);
        } else if (row.text != nullptr) {
            const int cells = seam_measure(row.text).cells, fit = r.w / cell;
            const int shown = cells < fit ? cells : fit;
            out_run(b, d, r.x + ((r.w - shown * cell) >> 1), ty, row.text, cut(row.text, shown), st.text, text_look.text);
        }
        return;
    }
    int x = r.x + 2;
    if (row.icon_texture >= 0) {
        out_sprite(b, d, row.icon_texture, row.icon, x, r.y + ((r.h - row.icon.h) >> 1), 1, 1, text_look.tint);
        x += row.icon.w + kUiGap;
    }
    int right = r.x + r.w - 2;
    if (row.number != kUiNoNumber) {
        char buf[48];
        const int n = ui_format_number(buf, 47, row.number, row.number_prefix != nullptr ? row.number_prefix : "");
        buf[n] = 0;
        right -= seam_measure(buf).cells * cell;
        out_run(b, d, right, ty, buf, n, st.text, text_look.text);
        right -= kUiGap;
    }
    if (row.text != nullptr && right > x) out_run(b, d, x, ty, row.text, cut(row.text, (right - x) / cell), st.text, text_look.text);
}

} // namespace

void draw_list(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st) {
    list_fill_rows(b, s);
    const UiListData& l = b.cold[s].data.list;
    const int cells = l.rows * l.columns;
    const int end = l.first + cells < l.count ? l.first + cells : l.count;
    for (int i = l.first; i < end; ++i) draw_row(b, d, s, st, i, l.columns > 1);
    const int total = (l.count + l.columns - 1) / l.columns;
    if (total <= l.rows || st.list.scroll_width == 0) return;
    const UiRect a = list_area(b, s);
    const UiRect track{a.x + a.w, a.y, st.list.scroll_width, a.h};
    out_background(b, d, track, st.list.track);
    int thumb = track.h * l.rows / total;
    thumb = thumb < 4 ? 4 : thumb;
    const int y = track.y + (track.h - thumb) * (l.first / l.columns) / (total - l.rows);
    out_background(b, d, {track.x, y, track.w, thumb}, st.list.thumb);
}

} // namespace nnh::detail
