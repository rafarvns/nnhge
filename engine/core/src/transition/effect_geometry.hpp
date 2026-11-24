#pragma once
// Effect geometry (spec 023 FR-4..FR-6, FR-9; plan 023 "Geometry"): every built-in effect as whole-pixel
// rectangles computed in integers from a 16.16 coverage. Pure: no engine state, no allocation, no libm; the
// rectangles go to a sink one at a time (no buffer of its own). Tests: tests/transition/test_effect_*.cpp.

#include <cstdint>

#include "nnh/transition.hpp"

namespace nnh::detail {

/// A rectangle of whole pixels.
struct PixelRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

/// Receives each rectangle, in drawing order.
using RectSink = void (*)(void* ctx, const PixelRect& rect);

/// Full coverage in 16.16: q = ⌊c · 65536 + ½⌋.
inline constexpr int kFullQ = 65536;

/// q of a coverage: clamped to 0..1 first (NaN → 0). Exact in IEEE single precision.
int coverage_q(float coverage);

/// ext(L) = (q · L + 32768) >> 16: the part of a length L covered at q (0 at q = 0, L at q = 65536).
/// Unsigned 32-bit: q ≤ 65536 and L < 65536 never overflow.
constexpr int ext(int q, int length) {
    return static_cast<int>((static_cast<std::uint32_t>(q) * static_cast<std::uint32_t>(length) + 32768u) >> 16);
}

/// An effect resolved for one screen (or the tall area): plain bytes, settings clamped, the iris centre in area
/// pixels.
struct ResolvedEffect {
    transition::Kind kind = transition::Kind::None;
    std::uint8_t dir = 0;    ///< Edge (wipe, blinds) or SplitAxis (split)
    std::uint8_t shape = 0;  ///< IrisShape
    std::uint8_t bars = 8;   ///< 2..32
    std::uint8_t band = 4;   ///< 1..16
    bool continued = false;  ///< drawn as a Reveal::Continue reveal
    std::int16_t cx = 0;     ///< iris centre, area pixels
    std::int16_t cy = 0;
    Color color{};
};

/// Where the rectangles go: clipped to [x0, x1) × [y0, y1) of the area, then moved by (dx, dy).
struct RectTarget {
    RectSink sink = nullptr;
    void* ctx = nullptr;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int dx = 0, dy = 0;

    /// Clips, moves and sends one rectangle (nothing when it is clipped away or empty).
    void emit(int x, int y, int w, int h) const;
};

/// Draws `fx` (Fade, Wipe, Split or Blinds) on an area of w × h pixels at q. effect_fill.cpp.
void geometry_fill(const ResolvedEffect& fx, int q, int w, int h, const RectTarget& target);
/// Draws an iris on an area of w × h pixels at q, visiting only the bands inside the target's rows. effect_iris.cpp.
void geometry_iris(const ResolvedEffect& fx, int q, int w, int h, const RectTarget& target);
/// The exact integer square root ⌊√v⌋ (bit method: no libm, no VFP). effect_iris.cpp.
std::uint32_t isqrt(std::uint32_t v);

/// The area and the screens (effect_area.cpp).
struct EffectArea {
    bool tall = false;
    int gap = 0; ///< 0..240
};
inline constexpr int kTallBottomInset = 40; ///< the bottom screen's x in the tall area

/// Resolves a public effect for `screen` in `area` (settings clamped, centre rounded and placed, colour opaque).
ResolvedEffect resolve_effect(const transition::Effect& effect, Screen screen, EffectArea area, bool continued);
/// Draws a resolved built-in effect on `screen` at q into the sink (screen coordinates). Fade, Wipe, Split, Blinds,
/// Iris; anything else draws nothing. Directional effects and the iris use the tall area when `area.tall`.
void draw_screen_effect(const ResolvedEffect& fx, Screen screen, EffectArea area, int q, RectSink sink, void* ctx);
/// The colour drawn at q (a fade's alpha = (q · 255 + 32768) >> 16; others opaque).
Color effect_color_q(const ResolvedEffect& fx, int q);
/// Records a resolved built-in effect with gfx::fill_rect (ignore_camera, `layer`). effect_query.cpp.
void draw_resolved(const ResolvedEffect& fx, Screen screen, EffectArea area, int q, int layer);
/// Most rectangles per screen for a resolved effect.
int max_rects_resolved(const ResolvedEffect& fx, bool tall);

/// What each screen shows in each phase (plan 023 "Resolving screens"): same() replaced by the effect it stands for.
struct ScreenEffects {
    transition::Effect top_out, top_in, bottom_out, bottom_in;
};
/// top out = effect; top in = reveal, else effect; bottom out = bottom, else effect; bottom in = bottom_reveal, else
/// (bottom given ? bottom out : top in). With `tall`, the bottom screen takes the top's effects.
ScreenEffects resolve_screens(const transition::Options& options);

} // namespace nnh::detail
