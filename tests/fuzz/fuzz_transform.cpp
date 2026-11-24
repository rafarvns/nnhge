// Fuzz target body for transforms (spec 022 T5.6, see fuzz_targets.hpp): random float bits (every special pattern
// included) into every transform function. Nothing may be undefined (ASan/UBSan on Linux); sin_cos of a finite angle
// is finite and within [−1, 1]; inverse refuses exactly the maps whose determinant's exponent is below 2⁻¹⁰⁰ or not
// finite; is_finite agrees with the bits; compose of a pure translation keeps the parent's axes.

#include "fuzz_targets.hpp"

#include <bit>
#include <cstdlib>
#include <cstring>

#include "nnh/transform.hpp"

namespace nnh::fuzz {
namespace {

bool finite(float f) { return (std::bit_cast<std::uint32_t>(f) & 0x7F800000u) != 0x7F800000u; }

} // namespace

int transform_target(const std::uint8_t* data, std::size_t size) {
    float f[16] = {};
    std::memcpy(f, data, size < sizeof f ? size : sizeof f);
    const Transform2D a{{f[0], f[1]}, f[2], {f[3], f[4]}};
    const Transform2D b{{f[5], f[6]}, f[7], {f[8], f[9]}};
    if (finite(f[2])) {
        const SinCos sc = sin_cos(f[2]);
        if (!finite(sc.sin) || !finite(sc.cos) || sc.sin > 1.0f || sc.sin < -1.0f || sc.cos > 1.0f || sc.cos < -1.0f)
            std::abort();
    }
    const bool fa = finite(f[0]) && finite(f[1]) && finite(f[2]) && finite(f[3]) && finite(f[4]);
    if (is_finite(a) != fa) std::abort();
    if (!fa || !is_finite(b)) return 0; // the hierarchy treats these as the identity; values below need finite input
    const WorldTransform2D wa = to_world(a);
    const WorldTransform2D wb = compose(wa, b);
    const WorldTransform2D wt = compose(wa, {b.position, 0.0f, {1, 1}});
    if (std::memcmp(&wt.map.x, &wa.map.x, sizeof(Vec2) * 2) != 0) std::abort();
    (void)transform_point(wb.map, {f[10], f[11]});
    (void)transform_direction(wb.map, {f[12], f[13]});
    Affine2D inv{};
    const float det = wb.map.x.x * wb.map.y.y - wb.map.y.x * wb.map.x.y;
    const std::uint32_t e = (std::bit_cast<std::uint32_t>(det) >> 23) & 0xFFu;
    if (inverse(wb.map, inv) != (e >= 27u && e != 0xFFu)) std::abort();
    Vec2 local{};
    (void)to_local(wb.map, {f[14], f[15]}, local);
    Transform2D r{};
    (void)relative(wa, wb, r);
    (void)bounds(wb.map, {f[10], f[11], f[12], f[13]});
    return 0;
}

} // namespace nnh::fuzz
