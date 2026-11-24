// Axis-aligned bounds of a local rectangle under a map (spec 022 FR-14, T1.5): unrotated maps give the mapped
// rectangle exactly (w = |a|·r.w), mirrored ones normalised, rotated ones the box enclosing the four corners, quarter
// turns exactly.

#include "nnh/transform.hpp"
#include "test.hpp"

#include <cmath>

using nnh::Affine2D;
using nnh::Rect;

namespace {

bool same(Rect a, Rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }
bool near(Rect a, Rect b, float eps) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.w - b.w) <= eps &&
           std::fabs(a.h - b.h) <= eps;
}

} // namespace

NNH_TEST(bounds_unrotated_is_the_mapped_rectangle) {
    const Affine2D m = nnh::to_world({.position = {100, 50}, .scale = {2, 3}}).map;
    CHECK(same(nnh::bounds(m, {-4, -2, 8, 4}), Rect{92, 44, 16, 12}));
    CHECK(same(nnh::bounds(Affine2D{}, {1.5f, 2.5f, 3, 4}), Rect{1.5f, 2.5f, 3, 4}));
}

NNH_TEST(bounds_mirrored_rectangles_are_normalised) {
    const Affine2D m = nnh::to_world({.position = {100, 50}, .scale = {-1, 1}}).map;
    CHECK(same(nnh::bounds(m, {2, -2, 8, 4}), Rect{90, 48, 8, 4}));
    const Affine2D both = nnh::to_world({.position = {0, 0}, .scale = {-2, -2}}).map;
    CHECK(same(nnh::bounds(both, {1, 1, 3, 3}), Rect{-8, -8, 6, 6}));
}

NNH_TEST(bounds_quarter_turns_are_exact) {
    const Affine2D m = nnh::to_world({.position = {10, 10}, .rotation = nnh::kQuarterTurn}).map;
    // (0, 0, 8, 4) turned a quarter clockwise: x in [6, 10], y in [10, 18].
    CHECK(same(nnh::bounds(m, {0, 0, 8, 4}), Rect{6, 10, 4, 8}));
    const Affine2D h = nnh::to_world({.position = {10, 10}, .rotation = nnh::kHalfTurn}).map;
    CHECK(same(nnh::bounds(h, {0, 0, 8, 4}), Rect{2, 6, 8, 4}));
}

NNH_TEST(bounds_rotated_encloses_the_corners) {
    const float r = 0.5f;
    const Affine2D m = nnh::to_world({.position = {0, 0}, .rotation = r}).map;
    const Rect b = nnh::bounds(m, {-5, -5, 10, 10});
    const float half = 5 * (std::cos(r) + std::sin(r));
    CHECK(near(b, Rect{-half, -half, 2 * half, 2 * half}, 1e-4f));
}
