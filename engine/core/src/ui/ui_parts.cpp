// Part rectangles (ui_parts.hpp).

#include "ui/ui_layout.hpp"
#include "ui/ui_parts.hpp"
#include "ui/ui_text_seam.hpp"

namespace nnh::detail {
namespace {

int label_width(UiBlock& b, int s) {
    const int w = seam_measure(b, style_of(b, s).text, text_of(b, s)).width;
    return w > 0 ? w + kUiGap : 0;
}

int text_width_of(UiBlock& b, int s, int v) {
    char buf[64];
    const int n = value_text(b, s, v, buf, 63);
    buf[n] = 0;
    return seam_measure(b, style_of(b, s).text, buf).width;
}

/// A choice's text arrow ("<" or ">"): a cell of the built-in font, else the glyph's width.
int text_arrow_width(const UiBlock& b, const UiStyle& st, const char* arrow) {
    if (!st.choice.text_arrows) return 0;
    return seam_builtin(st.text) ? seam_cell(st.text) : seam_width(b, st.text, arrow, 1);
}

} // namespace

UiRect content_rect(const UiBlock& b, int s) {
    const Pad p = padding_of(b, s);
    const UiHot& n = b.hot[s];
    return {n.x + p.l, n.y + p.t, n.w - p.l - p.r, n.h - p.t - p.b};
}

int slider_value_width(UiBlock& b, int s) {
    const UiSliderData& x = b.cold[s].data.slider;
    if (x.show_value == 0) return 0;
    const int a = text_width_of(b, s, x.min), z = text_width_of(b, s, x.max);
    return a > z ? a : z;
}

SliderParts slider_parts(UiBlock& b, int s) {
    const UiRect c = content_rect(b, s);
    const UiSliderData& x = b.cold[s].data.slider;
    const UiStyle& st = style_of(b, s);
    SliderParts p;
    const int lw = label_width(b, s);
    const int vw = slider_value_width(b, s);
    p.label = {c.x, c.y, lw > 0 ? lw - kUiGap : 0, c.h};
    p.value = {c.x + c.w - vw, c.y, vw, c.h};
    p.track = {c.x + lw, c.y, c.w - lw - (vw > 0 ? vw + kUiGap : 0), c.h};
    if (p.track.w < 1) p.track.w = 1;
    const int knob_w = st.slider.knob_width < 1 ? 1 : st.slider.knob_width;
    const int range = x.max - x.min;
    const int travel = p.track.w - knob_w > 0 ? p.track.w - knob_w : 0;
    const int kx = range > 0 ? static_cast<int>(static_cast<long long>(travel) * (x.value - x.min) / range) : 0;
    p.knob = {p.track.x + kx, c.y, knob_w, c.h};
    return p;
}

int slider_value_at(const UiBlock& b, int s, const UiRect& track, int x) {
    const UiSliderData& d = b.cold[s].data.slider;
    const int q = x < track.x ? 0 : (x >= track.x + track.w ? track.w - 1 : x - track.x);
    const long long range = static_cast<long long>(d.max) - d.min;
    const long long den = static_cast<long long>(track.w > 1 ? track.w - 1 : 1) * d.step;
    const long long num = static_cast<long long>(q) * range;
    const long long k = (2 * num + den) / (2 * den); // round half up (num ≥ 0)
    const long long v = d.min + k * d.step;
    return static_cast<int>(v > d.max ? d.max : v);
}

ChoiceParts choice_parts(UiBlock& b, int s) {
    const UiRect c = content_rect(b, s);
    const UiStyle& st = style_of(b, s);
    const int lw = label_width(b, s);
    const int aw_l = st.choice.left.texture >= 0 ? st.choice.left.region.w : text_arrow_width(b, st, "<");
    const int aw_r = st.choice.right.texture >= 0 ? st.choice.right.region.w : text_arrow_width(b, st, ">");
    ChoiceParts p;
    p.label = {c.x, c.y, lw > 0 ? lw - kUiGap : 0, c.h};
    p.left = {c.x + lw, c.y, aw_l, c.h};
    p.right = {c.x + c.w - aw_r, c.y, aw_r, c.h};
    p.option = {p.left.x + aw_l + kUiGap, c.y, p.right.x - kUiGap - (p.left.x + aw_l + kUiGap), c.h};
    return p;
}

} // namespace nnh::detail
