// Points, directions, inverses and finiteness (spec 022 FR-2). The inverse's refusal is an exponent test on the
// determinant's bits (|det| < 2⁻¹⁰⁰, or not finite), so it never depends on how a system treats subnormals.

#include "transform/sin_cos.hpp"

namespace nnh {
namespace {

/// The determinant's exponent field must be ≥ 27 (2⁻¹⁰⁰) and not all ones (infinity, NaN).
bool invertible(float det) {
    const std::uint32_t e = (detail::float_bits(det) >> 23) & 0xFFu;
    return e >= 27u && e != 0xFFu;
}

float determinant(const Affine2D& m) { return m.x.x * m.y.y - m.y.x * m.x.y; }

} // namespace

Vec2 transform_point(const Affine2D& map, Vec2 p) {
    return {(map.x.x * p.x + map.y.x * p.y) + map.origin.x, (map.x.y * p.x + map.y.y * p.y) + map.origin.y};
}

Vec2 transform_direction(const Affine2D& map, Vec2 d) {
    return {map.x.x * d.x + map.y.x * d.y, map.x.y * d.x + map.y.y * d.y};
}

bool inverse(const Affine2D& map, Affine2D& out) {
    const float det = determinant(map);
    if (!invertible(det)) return false;
    const float r = 1.0f / det;
    Affine2D inv;
    inv.x = {map.y.y * r, -map.x.y * r};
    inv.y = {-map.y.x * r, map.x.x * r};
    inv.origin = {-(inv.x.x * map.origin.x + inv.y.x * map.origin.y), -(inv.x.y * map.origin.x + inv.y.y * map.origin.y)};
    out = inv;
    return true;
}

bool to_local(const Affine2D& map, Vec2 world_point, Vec2& out) {
    const float det = determinant(map);
    if (!invertible(det)) return false;
    const float r = 1.0f / det;
    const float dx = world_point.x - map.origin.x, dy = world_point.y - map.origin.y;
    out = {(map.y.y * dx - map.y.x * dy) * r, (map.x.x * dy - map.x.y * dx) * r};
    return true;
}

bool is_finite(const Transform2D& t) {
    using detail::finite_bits;
    using detail::float_bits;
    return finite_bits(float_bits(t.position.x)) && finite_bits(float_bits(t.position.y)) &&
           finite_bits(float_bits(t.rotation)) && finite_bits(float_bits(t.scale.x)) &&
           finite_bits(float_bits(t.scale.y));
}

} // namespace nnh
