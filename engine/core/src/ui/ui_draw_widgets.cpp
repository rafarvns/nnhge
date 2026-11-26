// Per-kind drawing (spec 028 FR-5, FR-6, FR-25, FR-26): the state's background, then text runs (wrapped, aligned,
// revealed), icons, a toggle's box, a slider's track, fill, knob and value, a choice's arrows and option, a progress
// fill and its text, list rows (icon, text, number) and the scroll bar. Draw only reads the tree (caches aside: keyed
// texts resolve, rows coming into view are filled).

#include <cstring>

#include "nnh/texture.hpp"
#include "ui/ui_draw.hpp"
#include "ui/ui_focus.hpp"
#include "ui/ui_layout.hpp"
#include "ui/ui_number.hpp"
#include "ui/ui_parts.hpp"
#include "ui/ui_text_seam.hpp"
#include "text/text_draw.hpp"
#include "text/text_wrap.hpp"
#include "text/utf8.hpp"

namespace nnh::detail {

const char* option_text(UiBlock& b, int i); // ui_text_source.cpp
UiRect list_area(const UiBlock& b, int s);  // ui_list.cpp

namespace {

/// Bytes of the first `cells` characters of `text` (`length` bytes); a '\r' takes no cell, as gfx and the wrapper count.
int bytes_of_cells(const char* text, int length, int cells) {
    const char* p = text;
    for (int i = 0; i < cells && p < text + length;) {
        if (seam_next(p) != U'\r') ++i;
    }
    const int n = static_cast<int>(p - text);
    return n > length ? length : n;
}

/// One line of text, vertically centred in `r`, starting at x (cut to `max_cells` when > 0).
void line_in(const UiBlock& b, const DrawCtx& d, int x, const UiRect& r, const char* text, const UiStyle& st,
             Color color, int max_cells = 0) {
    const int line = seam_line(b, st.text);
    const int len = static_cast<int>(std::strlen(text));
    const int n = max_cells > 0 ? bytes_of_cells(text, len, max_cells) : len;
    out_run(b, d, x, r.y + ((r.h - line) >> 1), text, n, st.text, color);
}

int text_px(const UiBlock& b, const char* text, const UiStyle& st) { return seam_measure(b, st.text, text).width; }

void draw_label(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st, const UiLook& look) {
    const UiRect c = content_rect(b, s);
    const UiLabelData& l = b.cold[s].data.label;
    const char* text = text_of(b, s);
    const int line = seam_line(b, st.text);
    const TextFace face = seam_face(b, st.text);
    const bool builtin = face.builtin();
    const int cell = seam_cell(st.text);
    // the wrap width: whole cells of the built-in font (spec 028), else the content's pixels (at least one)
    const int wrap = (l.bits & 1) == 0 ? 0 : (builtin ? (c.w / cell > 0 ? c.w / cell : 1) * cell : (c.w > 0 ? c.w : 1));
    const auto align = static_cast<TextAlign>((l.bits >> 1) & 3u);
    const auto valign = static_cast<UiAlign>((l.bits >> 3) & 3u);
    const int height = l.lines * line;
    int y = valign == UiAlign::Center ? c.y + ((c.h - height) >> 1) : (valign == UiAlign::End ? c.y + c.h - height : c.y);
    if ((l.bits & 1) == 0 && l.max_lines == 0 && l.lines == 1 && l.visible == 0xFFFF) {
        // one whole line (the common label): the run the wrapper would give, from pass 1's count, with no walk
        const int full = builtin ? l.cells * cell : seam_measure(b, st.text, text).width;
        const int x = align == TextAlign::Center ? c.x + ((c.w - full) >> 1) : (align == TextAlign::Right ? c.x + c.w - full : c.x);
        out_run(b, d, x, y, text, static_cast<int>(std::strlen(text)), st.text, look.text, false);
        return;
    }
    int left = l.visible == 0xFFFF ? 0x7FFFFFFF : l.visible;
    TextWrapper w(face, text, static_cast<int>(std::strlen(text)), wrap, l.max_lines);
    WrapLine ln;
    while (w.next(ln) && left > 0) {
        const int full = text_line_width(face, ln) * face.scale; // built-in: (chars + 3 for "...") × cell
        const int x = align == TextAlign::Center ? c.x + ((c.w - full) >> 1) : (align == TextAlign::Right ? c.x + c.w - full : c.x);
        const int shown = ln.chars < left ? ln.chars : left;
        const bool all = shown == ln.chars;
        out_run(b, d, x, y, text + ln.start, bytes_of_cells(text + ln.start, ln.bytes, shown), st.text, look.text,
                ln.ellipsis && all);
        left -= ln.chars;
        y += line;
    }
}

void draw_button(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st, const UiLook& look) {
    const UiRect c = content_rect(b, s);
    const UiButtonData& x = b.cold[s].data.button;
    const char* text = text_of(b, s);
    const int text_w = text_px(b, text, st);
    const int icon_w = x.icon_texture >= 0 ? x.iw : 0;
    const int total = icon_w + (icon_w > 0 && text_w > 0 ? kUiGap : 0) + text_w;
    int at = c.x + ((c.w - total) >> 1);
    if (icon_w > 0) {
        out_sprite(b, d, x.icon_texture, {x.ix, x.iy, x.iw, x.ih}, at, c.y + ((c.h - x.ih) >> 1), 1, 1, look.tint);
        at += icon_w + kUiGap;
    }
    if (text_w > 0) line_in(b, d, at, c, text, st, look.text);
}

void draw_toggle(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st, const UiLook& look) {
    const UiRect c = content_rect(b, s);
    const bool on = b.cold[s].data.toggle.checked != 0;
    const UiRect box{c.x, c.y + ((c.h - kUiToggleBox) >> 1), kUiToggleBox, kUiToggleBox};
    const UiImagePart& img = on ? st.toggle.on : st.toggle.off;
    if (img.texture >= 0) {
        out_sprite(b, d, img.texture, img.region, box.x, c.y + ((c.h - img.region.h) >> 1), 1, 1, look.tint);
    } else {
        out_outline(b, d, box, look.text);
        if (on) out_fill(b, d, {box.x + 2, box.y + 2, box.w - 4, box.h - 4}, look.text);
    }
    line_in(b, d, c.x + kUiToggleBox + kUiGap, c, text_of(b, s), st, look.text);
}

void draw_slider(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st, const UiLook& look) {
    const UiRect c = content_rect(b, s);
    const SliderParts p = slider_parts(b, s);
    line_in(b, d, p.label.x, c, text_of(b, s), st, look.text);
    const int th = st.slider.track_height < 1 ? 1 : st.slider.track_height;
    const UiRect track{p.track.x, c.y + ((c.h - th) >> 1), p.track.w, th};
    out_background(b, d, track, st.slider.track);
    out_background(b, d, {track.x, track.y, p.knob.x - track.x, th}, st.slider.fill);
    out_background(b, d, p.knob, st.slider.knob);
    if (p.value.w > 0) {
        char buf[64];
        const int n = value_text(b, s, b.cold[s].data.slider.value, buf, 63);
        buf[n] = 0;
        line_in(b, d, p.value.x + p.value.w - text_px(b, buf, st), c, buf, st, look.text);
    }
}

void draw_arrow(UiBlock& b, const DrawCtx& d, const UiRect& r, const UiImagePart& img, const char* text,
                const UiStyle& st, const UiLook& look) {
    if (img.texture >= 0) {
        out_sprite(b, d, img.texture, img.region, r.x, r.y + ((r.h - img.region.h) >> 1), 1, 1, look.tint);
    } else if (st.choice.text_arrows) {
        line_in(b, d, r.x, r, text, st, look.text);
    }
}

void draw_choice(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st, const UiLook& look) {
    const UiRect c = content_rect(b, s);
    const ChoiceParts p = choice_parts(b, s);
    line_in(b, d, p.label.x, c, text_of(b, s), st, look.text);
    draw_arrow(b, d, p.left, st.choice.left, "<", st, look);
    const UiChoiceData& x = b.cold[s].data.choice;
    const char* option = option_text(b, x.first_text + x.index);
    line_in(b, d, p.option.x + ((p.option.w - text_px(b, option, st)) >> 1), c, option, st, look.text);
    draw_arrow(b, d, p.right, st.choice.right, ">", st, look);
}

void draw_progress(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st, const UiLook& look) {
    const UiRect c = content_rect(b, s);
    const UiProgressData& x = b.cold[s].data.progress;
    if ((x.bits & 1) != 0) {
        const int h = static_cast<int>(static_cast<long long>(c.h) * x.value / x.max);
        out_background(b, d, {c.x, c.y + c.h - h, c.w, h}, st.progress.fill);
    } else {
        out_background(b, d, {c.x, c.y, static_cast<int>(static_cast<long long>(c.w) * x.value / x.max), c.h},
                       st.progress.fill);
    }
    if ((x.bits & 2) != 0) {
        char buf[64];
        const int n = value_text(b, s, x.value, buf, 63);
        buf[n] = 0;
        line_in(b, d, c.x + ((c.w - text_px(b, buf, st)) >> 1), c, buf, st, look.text);
    }
}

void draw_image(UiBlock& b, const DrawCtx& d, int s, const UiLook& look) {
    const UiRect c = content_rect(b, s);
    const UiImageData& m = b.cold[s].data.image;
    if (m.texture < 0) return;
    const float k = static_cast<float>(m.scale);
    const Color t{static_cast<std::uint8_t>(m.tint.r * look.tint.r / 255), static_cast<std::uint8_t>(m.tint.g * look.tint.g / 255),
                  static_cast<std::uint8_t>(m.tint.b * look.tint.b / 255), static_cast<std::uint8_t>(m.tint.a * look.tint.a / 255)};
    out_sprite(b, d, m.texture, {m.rx, m.ry, m.rw, m.rh}, c.x, c.y, k, k, t);
}

} // namespace

void draw_list(UiBlock& b, const DrawCtx& d, int s, const UiStyle& st); // ui_draw_list.cpp

void draw_widget(const UiBlock& cb, const DrawCtx& d, int s) {
    UiBlock& b = const_cast<UiBlock&>(cb); // draw only reads; keyed texts and pooled rows are caches
    const UiKind k = kind_of(b, s);
    if (!has_look(k)) return;
    const UiStyle& st = style_of(b, s);
    const UiLook& look = st.states[static_cast<int>(state_of(b, s))];
    out_background(b, d, rect_of(b.hot[s]), look.background);
    switch (k) {
    case UiKind::Label: draw_label(b, d, s, st, look); break;
    case UiKind::Button: draw_button(b, d, s, st, look); break;
    case UiKind::Toggle: draw_toggle(b, d, s, st, look); break;
    case UiKind::Slider: draw_slider(b, d, s, st, look); break;
    case UiKind::Choice: draw_choice(b, d, s, st, look); break;
    case UiKind::Progress: draw_progress(b, d, s, st, look); break;
    case UiKind::Image: draw_image(b, d, s, look); break;
    case UiKind::List:
    case UiKind::Grid: draw_list(b, d, s, st); break;
    default: break;
    }
}

} // namespace nnh::detail
