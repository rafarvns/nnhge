#pragma once
// Exact predicates of the queries (spec 019, D34): a segment in double (exact differences of the float inputs),
// ratios along it compared by cross products, results rounded once to float, and one-ulp float steps on the bit
// pattern. For coordinates that are multiples of 1/256 px within ±2^17 px every comparison is exact; beyond that
// it is still plain IEEE arithmetic, the same on every system. Internal, pure; compiled with -ffp-contract=off.

#include "nnh/collision.hpp"

namespace nnh::collision::detail {

/// t = n / a with a > 0: a position along a segment (0 = from, 1 = to).
struct Ratio {
    double n = 0;
    double a = 1;
};
inline constexpr Ratio kRatioZero{0, 1};
inline constexpr Ratio kRatioOne{1, 1};

/// −1, 0 or 1 as p is before, at or after q (n·q.a against q.n·p.a, no division).
int compare(Ratio p, Ratio q);

/// The segment from → to: start, delta, its magnitude and sign per axis (all exact in double).
struct Segment {
    double px = 0, py = 0;
    double dx = 0, dy = 0;
    double ax = 0, ay = 0; ///< |dx|, |dy|
    int sx = 0, sy = 0;    ///< −1, 0, 1
};
Segment make_segment(Vec2 from, Vec2 to);
inline bool zero_length(const Segment& s) { return s.sx == 0 && s.sy == 0; }

/// `t` rounded once to float.
float to_fraction(Ratio t);
/// p + d·t, rounded once to float.
float along(double p, double d, Ratio t);
inline float clamp_to(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// The next float above / below `v` by its bit pattern (±0 → ± the smallest subnormal; infinities and NaN are
/// returned unchanged). No <cmath>, no nextafter.
float step_up(float v);
float step_down(float v);

} // namespace nnh::collision::detail
