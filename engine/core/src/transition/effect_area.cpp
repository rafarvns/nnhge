// Screens and the tall area (plan 023 "Tall area → screens"): an effect is resolved for a screen (settings clamped,
// the iris centre rounded half up and placed in area pixels), computed once in area coordinates and clipped to the
// screen. The tall area is 400 × (480 + gap): the top screen at (0, 0), the bottom one at (40, 240 + gap); the gap
// is drawn nowhere.

#include "transition/effect_geometry.hpp"

namespace nnh::detail {
namespace {

using transition::Edge;
using transition::IrisShape;
using transition::Kind;
using transition::SplitAxis;

constexpr float kMaxCentre = 4096.0f; // keeps r² in 32 bits (plan 023 "Requests")
constexpr int kScreenH = 240;

int clamp_int(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// ⌊v + ½⌋ for |v| ≤ 4096 (no libm).
int round_half_up(float v) {
    const float t = v + 0.5f;
    int i = static_cast<int>(t);
    if (static_cast<float>(i) > t) --i;
    return i;
}

int area_height(EffectArea area) { return 2 * kScreenH + area.gap; }

bool uses_tall_area(const ResolvedEffect& fx, EffectArea area) {
    return area.tall && (fx.kind == Kind::Wipe || fx.kind == Kind::Split || fx.kind == Kind::Blinds ||
                         fx.kind == Kind::Iris);
}

void resolve_centre(const transition::IrisCenter& c, Screen screen, EffectArea area, ResolvedEffect& fx) {
    const int w = area.tall ? screen_width(Screen::Top) : screen_width(screen);
    const int h = area.tall ? area_height(area) : kScreenH;
    const float x = c.point.x;
    const float y = c.point.y;
    // NaN and infinities fail both comparisons (asserted by the requests; harmless here): the middle.
    if (!c.given || !(x >= -1e30f && x <= 1e30f) || !(y >= -1e30f && y <= 1e30f)) {
        fx.cx = static_cast<std::int16_t>(w / 2);
        fx.cy = static_cast<std::int16_t>(h / 2);
        return;
    }
    int px = round_half_up(x < -kMaxCentre ? -kMaxCentre : (x > kMaxCentre ? kMaxCentre : x));
    int py = round_half_up(y < -kMaxCentre ? -kMaxCentre : (y > kMaxCentre ? kMaxCentre : y));
    if (area.tall && c.screen == Screen::Bottom) {
        px += kTallBottomInset;
        py += kScreenH + area.gap;
    }
    fx.cx = static_cast<std::int16_t>(px);
    fx.cy = static_cast<std::int16_t>(py);
}

} // namespace

int coverage_q(float coverage) {
    if (!(coverage > 0.0f)) return 0; // NaN too
    if (coverage >= 1.0f) return kFullQ;
    return static_cast<int>(coverage * 65536.0f + 0.5f); // exact: c · 2¹⁶ < 2¹⁷ keeps every bit
}

void RectTarget::emit(int x, int y, int w, int h) const {
    const int left = x > x0 ? x : x0;
    const int top = y > y0 ? y : y0;
    const int right = x + w < x1 ? x + w : x1;
    const int bottom = y + h < y1 ? y + h : y1;
    if (right <= left || bottom <= top) return;
    sink(ctx, PixelRect{left + dx, top + dy, right - left, bottom - top});
}

ResolvedEffect resolve_effect(const transition::Effect& effect, Screen screen, EffectArea area, bool continued) {
    ResolvedEffect fx;
    const auto kind = static_cast<std::uint8_t>(effect.kind);
    fx.kind = kind <= static_cast<std::uint8_t>(Kind::Custom) ? effect.kind : Kind::None;
    const auto edge = static_cast<std::uint8_t>(effect.edge);
    const auto axis = static_cast<std::uint8_t>(effect.split);
    if (fx.kind == Kind::Split) {
        fx.dir = axis <= static_cast<std::uint8_t>(SplitAxis::TopBottom) ? axis : 0;
    } else {
        fx.dir = edge <= static_cast<std::uint8_t>(Edge::Bottom) ? edge : 0;
    }
    const auto shape = static_cast<std::uint8_t>(effect.shape);
    fx.shape = shape <= static_cast<std::uint8_t>(IrisShape::Diamond) ? shape : 0;
    fx.bars = static_cast<std::uint8_t>(clamp_int(effect.bars, 2, 32));
    fx.band = static_cast<std::uint8_t>(clamp_int(effect.band, 1, 16));
    fx.continued = continued;
    fx.color = effect.color;
    resolve_centre(effect.center, screen, area, fx);
    return fx;
}

void draw_screen_effect(const ResolvedEffect& fx, Screen screen, EffectArea area, int q, RectSink sink, void* ctx) {
    const int sw = screen_width(screen);
    RectTarget t{sink, ctx, 0, 0, sw, kScreenH, 0, 0};
    int w = sw;
    int h = kScreenH;
    if (uses_tall_area(fx, area)) {
        w = screen_width(Screen::Top);
        h = area_height(area);
        if (screen == Screen::Bottom) {
            const int top = kScreenH + area.gap;
            t = RectTarget{sink, ctx, kTallBottomInset, top, kTallBottomInset + sw, top + kScreenH,
                           -kTallBottomInset, -top};
        }
    }
    if (fx.kind == Kind::Iris) {
        geometry_iris(fx, q, w, h, t);
    } else {
        geometry_fill(fx, q, w, h, t);
    }
}

ScreenEffects resolve_screens(const transition::Options& o) {
    ScreenEffects s;
    s.top_out = o.effect;
    s.top_in = o.reveal.kind != Kind::Same ? o.reveal : o.effect;
    if (o.tall) { // the effects span both screens: the bottom screen takes the top's (asserted by the requests)
        s.bottom_out = s.top_out;
        s.bottom_in = s.top_in;
        return s;
    }
    const bool bottom_given = o.bottom.kind != Kind::Same;
    s.bottom_out = bottom_given ? o.bottom : o.effect;
    s.bottom_in = o.bottom_reveal.kind != Kind::Same ? o.bottom_reveal : (bottom_given ? s.bottom_out : s.top_in);
    return s;
}

int max_rects_resolved(const ResolvedEffect& fx, bool tall) {
    switch (fx.kind) {
    case Kind::Fade:
    case Kind::Wipe: return 1;
    case Kind::Split: return 2;
    case Kind::Blinds: return fx.bars;
    case Kind::Iris: {
        const int b = fx.band;
        // In the tall area a band may straddle the bottom screen's top edge: one more band.
        const int bands = tall ? (kScreenH + b - 2) / b + 1 : (kScreenH + b - 1) / b;
        return 2 * bands + 2;
    }
    default: return 0;
    }
}

} // namespace nnh::detail
