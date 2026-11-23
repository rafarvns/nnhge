// Exact predicates (spec 019, D34), see exact.hpp.

#include "collision/exact.hpp"

#include <cstdint>
#include <cstring>

namespace nnh::collision::detail {

namespace {

int sign_of(double v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

std::uint32_t bits_of(float v) {
    std::uint32_t b;
    std::memcpy(&b, &v, sizeof b);
    return b;
}

float from_bits(std::uint32_t b) {
    float v;
    std::memcpy(&v, &b, sizeof v);
    return v;
}

} // namespace

int compare(Ratio p, Ratio q) {
    const double l = p.n * q.a;
    const double r = q.n * p.a;
    return l < r ? -1 : (l > r ? 1 : 0);
}

Segment make_segment(Vec2 from, Vec2 to) {
    Segment s;
    s.px = from.x;
    s.py = from.y;
    s.dx = static_cast<double>(to.x) - s.px;
    s.dy = static_cast<double>(to.y) - s.py;
    s.sx = sign_of(s.dx);
    s.sy = sign_of(s.dy);
    s.ax = s.sx < 0 ? -s.dx : s.dx;
    s.ay = s.sy < 0 ? -s.dy : s.dy;
    return s;
}

float to_fraction(Ratio t) { return static_cast<float>(t.n / t.a); }

float along(double p, double d, Ratio t) { return static_cast<float>(p + d * t.n / t.a); }

float step_up(float v) {
    if (!(v - v == 0)) return v; // NaN, ±infinity
    const std::uint32_t b = bits_of(v);
    if ((b & 0x7FFFFFFFu) == 0) return from_bits(1u); // ±0 → the smallest subnormal
    return from_bits((b & 0x80000000u) != 0 ? b - 1 : b + 1);
}

float step_down(float v) { return -step_up(-v); }

} // namespace nnh::collision::detail
