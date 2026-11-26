// Ranges and stepping (see live_steps.hpp). Built with -ffp-contract=off: g(k) = min + float(k) * step is two
// roundings on every compiler and system. Estimates use double division (a VFP instruction on the 3DS, no __aeabi_*
// call), then exact comparisons settle the index, so the result never depends on the estimate.

#include "live/live_steps.hpp"

#include <cmath>

namespace nnh::detail::live {
namespace {

bool finite(float v) { return std::isfinite(v); }

// Grid points in a type that compares exactly with values and never overflows: float, or int64 for ints.
float point(const FloatGrid& g, std::uint32_t k) { return grid_value(g, k); }
std::int64_t point(const IntGrid& g, std::uint32_t k) {
    return static_cast<std::int64_t>(g.min) + static_cast<std::int64_t>(k) * g.step;
}
float wide(float v) { return v; }
std::int64_t wide(std::int32_t v) { return v; }

/// An index estimate clamped to [0, limit].
std::uint32_t clamp_index(double estimate, std::uint32_t limit) {
    if (!(estimate > 0)) return 0;
    if (estimate >= static_cast<double>(limit)) return limit;
    return static_cast<std::uint32_t>(estimate);
}

/// The smallest k in 0..n with g(k) > v, or n + 1.
template <typename Grid, typename V>
std::uint32_t first_above(const Grid& g, V v) {
    std::uint32_t k = clamp_index((static_cast<double>(v) - static_cast<double>(g.min)) / static_cast<double>(g.step),
                                  g.n + 1);
    while (k > 0 && point(g, k - 1) > wide(v)) --k;
    while (k <= g.n && !(point(g, k) > wide(v))) ++k;
    return k;
}

/// The largest k in 0..n with g(k) < v, or -1.
template <typename Grid, typename V>
std::int64_t last_below(const Grid& g, V v) {
    const std::uint32_t above = first_above(g, v); // every g(k) < above ... g(above - 1) <= v
    std::int64_t k = static_cast<std::int64_t>(above) - 1;
    while (k >= 0 && !(point(g, static_cast<std::uint32_t>(k)) < wide(v))) --k;
    return k;
}

template <typename Grid, typename V>
V step_on_grid(const Grid& g, V v, int delta) {
    if (delta > 0) {
        const std::uint64_t k = static_cast<std::uint64_t>(first_above(g, v)) + static_cast<std::uint64_t>(delta - 1);
        return k > g.n ? g.max : grid_value(g, static_cast<std::uint32_t>(k));
    }
    const std::int64_t k = last_below(g, v) - static_cast<std::int64_t>(-delta - 1);
    return k < 0 ? g.min : grid_value(g, static_cast<std::uint32_t>(k));
}

/// n: the largest k <= 2^24 with g(k) <= max, from an estimate settled by comparisons. False when (max − min) / step
/// (in double) reaches 2^24 + 1.
template <typename Grid>
bool count_steps(Grid& g) {
    // The span decides the limit (float grid points past 2^24 are not exact, so they can't).
    const double span = (static_cast<double>(g.max) - static_cast<double>(g.min)) / static_cast<double>(g.step);
    if (span >= static_cast<double>(kMaxGridSteps) + 1) return false;
    g.n = clamp_index(span, kMaxGridSteps);
    while (g.n > 0 && point(g, g.n) > wide(g.max)) --g.n;
    while (g.n < kMaxGridSteps && !(point(g, g.n + 1) > wide(g.max))) ++g.n;
    return true;
}

} // namespace

RangeError make_float_grid(float min, float max, float step, FloatGrid& out) {
    if (!finite(min) || !finite(max)) return RangeError::NotFinite;
    if (!(min < max)) return RangeError::Empty;
    if (!finite(step) || !(step > 0)) return RangeError::BadStep;
    FloatGrid g{min, max, step, 0};
    if (!count_steps(g)) return RangeError::TooManySteps;
    out = g;
    return RangeError::None;
}

RangeError make_int_grid(std::int32_t min, std::int32_t max, std::int32_t step, IntGrid& out) {
    if (!(min < max)) return RangeError::Empty;
    if (step <= 0) return RangeError::BadStep;
    IntGrid g{min, max, step, 0};
    if (!count_steps(g)) return RangeError::TooManySteps;
    out = g;
    return RangeError::None;
}

float grid_value(const FloatGrid& g, std::uint32_t k) { return k == 0 ? g.min : g.min + static_cast<float>(k) * g.step; }

std::int32_t grid_value(const IntGrid& g, std::uint32_t k) {
    const std::int64_t v = point(g, k);
    return v > g.max ? g.max : static_cast<std::int32_t>(v);
}

float step_value(const FloatGrid& g, float v, int delta) { return step_on_grid(g, v, delta); }
std::int32_t step_value(const IntGrid& g, std::int32_t v, int delta) { return step_on_grid(g, v, delta); }

std::uint32_t drag_index(int x, int x0, int w, std::uint32_t n) {
    if (w <= 0) return 0;
    int dx = x - x0;
    dx = dx < 0 ? 0 : (dx > w ? w : dx);
    // ((dx · n) + w / 2) / w: the numerator is below 2^34, exact in double, and the quotient's distance to the next
    // integer (>= 1/w) is far above the division's rounding, so floor() gives the integer quotient.
    const double num = static_cast<double>(dx) * static_cast<double>(n) + static_cast<double>(w >> 1);
    return static_cast<std::uint32_t>(std::floor(num / static_cast<double>(w)));
}

float drag_value(const FloatGrid& g, int x, int x0, int w) { return grid_value(g, drag_index(x, x0, w, g.n)); }
std::int32_t drag_value(const IntGrid& g, int x, int x0, int w) { return grid_value(g, drag_index(x, x0, w, g.n)); }

int bar_fill(const FloatGrid& g, float v, int w) {
    const float t = (v - g.min) / (g.max - g.min) * static_cast<float>(w);
    if (!(t > 0)) return 0;
    return t >= static_cast<float>(w) ? w : static_cast<int>(t);
}

int bar_fill(const IntGrid& g, std::int32_t v, int w) {
    if (v <= g.min) return 0;
    // Both differences fit 32 unsigned bits: a VFP conversion, no __aeabi_l2f call (T7.7).
    const auto num = static_cast<std::uint32_t>(static_cast<std::uint32_t>(v) - static_cast<std::uint32_t>(g.min));
    const auto den = static_cast<std::uint32_t>(static_cast<std::uint32_t>(g.max) - static_cast<std::uint32_t>(g.min));
    const float t = static_cast<float>(num) / static_cast<float>(den) * static_cast<float>(w);
    if (!(t > 0)) return 0;
    return t >= static_cast<float>(w) ? w : static_cast<int>(t);
}

std::uint8_t step_channel(std::uint8_t v, int delta) {
    const int r = static_cast<int>(v) + delta;
    return static_cast<std::uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r));
}

std::uint32_t step_enum(std::uint32_t index, std::uint32_t count, int delta) {
    if (count == 0) return 0;
    const std::int64_t r = static_cast<std::int64_t>(index) + delta;
    const std::int64_t last = static_cast<std::int64_t>(count) - 1;
    return static_cast<std::uint32_t>(r < 0 ? 0 : (r > last ? last : r));
}

Refusal check_value(const ValueRule& rule, std::uint32_t bits) {
    switch (rule.kind) {
    case Kind::Float: {
        const float v = bits_float(bits);
        if (!finite(v)) return Refusal::NotFinite;
        return v >= bits_float(rule.min) && v <= bits_float(rule.max) ? Refusal::None : Refusal::OutOfRange;
    }
    case Kind::Int32: {
        const auto v = static_cast<std::int32_t>(bits);
        return v >= static_cast<std::int32_t>(rule.min) && v <= static_cast<std::int32_t>(rule.max)
                   ? Refusal::None
                   : Refusal::OutOfRange;
    }
    case Kind::Bool: return bits <= 1 ? Refusal::None : Refusal::OutOfRange;
    case Kind::Enum: return bits < rule.count ? Refusal::None : Refusal::OutOfRange;
    case Kind::Color: return Refusal::None;
    default: return Refusal::WrongKind;
    }
}

const char* refusal_text(Refusal r) {
    switch (r) {
    case Refusal::None: return "accepted";
    case Refusal::NotDeclared: return "not declared";
    case Refusal::WrongKind: return "wrong kind";
    case Refusal::OutOfRange: return "out of range";
    case Refusal::NotFinite: return "not finite";
    case Refusal::QueueFull: return "queue full";
    case Refusal::FollowsFull: return "follows full";
    case Refusal::NoLiveValues: return "no live values in this game";
    }
    return "unknown";
}

} // namespace nnh::detail::live
