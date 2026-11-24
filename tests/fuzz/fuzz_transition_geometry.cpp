// Fuzz target of the transition effect geometry (spec 023 T3.5, constitution X): any effect fields (enum bytes out
// of range, any bar and band counts, NaN, infinite and huge iris centres), any coverage bits and any placement draw
// only rectangles inside their screen, no more than max_rects() and kMaxEffectRects, with no undefined behaviour.

#include "fuzz_transition_geometry.hpp"

#include <cstdlib>
#include <cstring>

#include "nnh/transition.hpp"

namespace nnh::fuzz {
namespace {

constexpr std::size_t kRecord = 26;

template <typename T>
T take(const std::uint8_t* p) {
    T v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

bool rects_ok(Screen screen, const Rect* rects, int n) {
    const float w = static_cast<float>(screen_width(screen));
    for (int i = 0; i < n; ++i) {
        const Rect& r = rects[i];
        if (!(r.w > 0 && r.h > 0 && r.x >= 0 && r.y >= 0 && r.x + r.w <= w && r.y + r.h <= 240)) return false;
    }
    return true;
}

} // namespace

int transition_geometry_target(const std::uint8_t* data, std::size_t size) {
    static Rect rects[transition::kMaxEffectRects];
    for (std::size_t at = 0; at + kRecord <= size; at += kRecord) {
        const std::uint8_t* p = data + at;
        transition::Effect e;
        std::memcpy(&e.kind, p + 0, 1); // any byte, even outside the enum (the geometry must not trust it)
        e.color = Color{p[1], p[2], p[3], p[4]};
        std::memcpy(&e.edge, p + 5, 1);
        std::memcpy(&e.split, p + 6, 1);
        e.bars = static_cast<std::int8_t>(p[7]);
        std::memcpy(&e.shape, p + 8, 1);
        e.band = static_cast<std::int8_t>(p[9]);
        e.center.point = Vec2{take<float>(p + 10), take<float>(p + 14)};
        e.center.screen = (p[18] & 1) != 0 ? Screen::Bottom : Screen::Top;
        e.center.given = (p[18] & 2) != 0;
        const float coverage = take<float>(p + 19);
        transition::Placement placement;
        placement.tall = (p[23] & 1) != 0;
        placement.continued = (p[23] & 2) != 0;
        placement.tall_gap = static_cast<std::int16_t>(take<std::uint16_t>(p + 24));
        const int max = transition::max_rects(e, placement.tall);
        for (Screen screen : {Screen::Top, Screen::Bottom}) {
            const int n = transition::effect_rects(screen, e, coverage, rects, transition::kMaxEffectRects, placement);
            if (n < 0 || n > max || n > transition::kMaxEffectRects || !rects_ok(screen, rects, n)) std::abort();
        }
        (void)transition::effect_color(e, coverage);
    }
    return 0;
}

} // namespace nnh::fuzz
