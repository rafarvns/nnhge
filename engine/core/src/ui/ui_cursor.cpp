// The focus cursor (spec 028 FR-27): its target is the focused widget's (or focused list item's) anchor point; when
// the target changes it glides there over move_frames with the style's easing, one ease() per frame rounded once
// (floor(x + 0.5)), so every system gives the same pixels; a pulse cycles its opacity and x offset in whole frames.

#include <cmath>

#include "ui/ui_block.hpp"
#include "ui/ui_cursor.hpp"

namespace nnh::detail {
namespace {

/// The point of `r` at anchor `a` (start, centre rounded down, end on each axis).
void anchor_point(const UiRect& r, UiAnchor a, int& x, int& y) {
    const int i = static_cast<int>(a) < 9 ? static_cast<int>(a) : 0;
    const int px = i % 3, py = i / 3;
    x = r.x + (px == 0 ? 0 : (px == 1 ? r.w >> 1 : r.w));
    y = r.y + (py == 0 ? 0 : (py == 1 ? r.h >> 1 : r.h));
}

} // namespace

UiRect cursor_target_rect(const UiBlock& b, int s) {
    if (is_list(kind_of(b, s))) {
        const UiRect r = list_item_rect(b, s, b.cold[s].data.list.focused);
        if (r.w > 0) return r;
    }
    return rect_of(b.hot[s]);
}

int cursor_lerp(int from, int to, float eased) {
    const float v = static_cast<float>(from) + static_cast<float>(to - from) * eased;
    return static_cast<int>(std::floor(v + 0.5f));
}

void cursor_step(UiBlock& b) {
    UiCursor& c = b.cursor;
    const UiCursorStyle& st = b.theme->cursor;
    if (st.pulse_frames > 0) c.pulse = static_cast<std::uint16_t>(c.pulse + 1 >= st.pulse_frames ? 0 : c.pulse + 1);
    if (b.focus == kUiNone) {
        c.placed = false;
        c.target = kUiNone;
        return;
    }
    int tx = 0, ty = 0;
    anchor_point(cursor_target_rect(b, b.focus), st.anchor, tx, ty);
    if (!c.placed) { // the first target: no glide
        c.placed = true;
        c.from_x = c.to_x = c.x = static_cast<std::int16_t>(tx);
        c.from_y = c.to_y = c.y = static_cast<std::int16_t>(ty);
        c.frame = st.move_frames;
        c.target = b.focus;
        return;
    }
    if (tx != c.to_x || ty != c.to_y) {
        c.from_x = c.x;
        c.from_y = c.y;
        c.to_x = static_cast<std::int16_t>(tx);
        c.to_y = static_cast<std::int16_t>(ty);
        c.frame = 0;
        c.target = b.focus;
    }
    if (c.frame < st.move_frames) ++c.frame;
    if (st.move_frames == 0 || c.frame >= st.move_frames) {
        c.x = c.to_x;
        c.y = c.to_y;
        return;
    }
    const float t = static_cast<float>(c.frame) / static_cast<float>(st.move_frames);
    const float e = ease(st.ease, t);
    c.x = static_cast<std::int16_t>(cursor_lerp(c.from_x, c.to_x, e));
    c.y = static_cast<std::int16_t>(cursor_lerp(c.from_y, c.to_y, e));
}

CursorPulse cursor_pulse(const UiBlock& b) {
    const UiCursorStyle& st = b.theme->cursor;
    CursorPulse p;
    if (st.pulse_frames < 2) return p;
    const int half = st.pulse_frames / 2;
    const int phase = b.cursor.pulse;
    const int amount = phase < half ? phase : st.pulse_frames - phase; // 0..half..0
    const int span = amount > half ? half : amount;
    p.opacity = 255 - (255 - st.pulse_opacity) * span / half;
    p.dx = st.pulse_x * span / half;
    return p;
}

} // namespace nnh::detail
