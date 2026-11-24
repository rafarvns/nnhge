#pragma once
// Composition (spec 022 FR-1, FR-2, FR-4; plan 022 D37), inline for the hierarchy's pass and world_now: the root rule,
// the pure-translation rule and the full composition, each a fixed sequence of float operations under
// -ffp-contract=off. The rule is chosen by the local's bits, so the pass, world_now and compose() agree bit for bit.

#include <bit>
#include <cstdint>

#include "nnh/transform.hpp"
#include "transform/sin_cos.hpp"

namespace nnh::detail {

inline constexpr std::uint32_t kOneBits = 0x3F800000u;

/// Rotation +0 and scale exactly (1, 1): the parent's axes and drawing values are kept.
NNH_DET_INLINE bool pure_translation(const Transform2D& local) {
    return float_bits(local.rotation) == 0u && float_bits(local.scale.x) == kOneBits &&
           float_bits(local.scale.y) == kOneBits;
}

/// A root: map.x = (c·sx, s·sx), map.y = (−s·sy, c·sy), origin = position; drawing values = the local's bits.
NNH_DET_INLINE void root_world(const Transform2D& local, SinCos sc, WorldTransform2D& out) {
    out.map.x.x = sc.cos * local.scale.x;
    out.map.x.y = sc.sin * local.scale.x;
    out.map.y.x = -sc.sin * local.scale.y;
    out.map.y.y = sc.cos * local.scale.y;
    out.map.origin = local.position;
    out.rotation = local.rotation;
    out.scale = local.scale;
}

/// origin = (P.x·px + P.y·py) + P.origin, component-wise, in this order.
NNH_DET_INLINE Vec2 composed_origin(const Affine2D& p, Vec2 position) {
    return {(p.x.x * position.x + p.y.x * position.y) + p.origin.x,
            (p.x.y * position.x + p.y.y * position.y) + p.origin.y};
}

/// A child of `p` with a pure-translation local.
NNH_DET_INLINE void translate_world(const WorldTransform2D& p, Vec2 position, WorldTransform2D& out) {
    out.map.x = p.map.x;
    out.map.y = p.map.y;
    out.map.origin = composed_origin(p.map, position);
    out.rotation = p.rotation;
    out.scale = p.scale;
}

/// The full composition: the parent's axes turned by the local rotation, then scaled; the drawing rotation turned the
/// other way under a mirrored parent (sign bits of its drawing scale); the drawing scale the product.
NNH_DET_INLINE void full_world(const WorldTransform2D& p, const Transform2D& local, SinCos sc, WorldTransform2D& out) {
    const Vec2 px = p.map.x, py = p.map.y;
    const float xr_x = px.x * sc.cos + py.x * sc.sin, xr_y = px.y * sc.cos + py.y * sc.sin;
    const float yr_x = py.x * sc.cos - px.x * sc.sin, yr_y = py.y * sc.cos - px.y * sc.sin;
    out.map.origin = composed_origin(p.map, local.position);
    out.map.x.x = xr_x * local.scale.x;
    out.map.x.y = xr_y * local.scale.x;
    out.map.y.x = yr_x * local.scale.y;
    out.map.y.y = yr_y * local.scale.y;
    const std::uint32_t mirror = (float_bits(p.scale.x) ^ float_bits(p.scale.y)) & kSignBit;
    out.rotation = p.rotation + std::bit_cast<float>(float_bits(local.rotation) ^ mirror);
    out.scale.x = p.scale.x * local.scale.x;
    out.scale.y = p.scale.y * local.scale.y;
}

/// A child's world from its parent's and its local, the rule chosen by the local's bits.
NNH_DET_INLINE void child_world(const WorldTransform2D& p, const Transform2D& local, SinCos sc, WorldTransform2D& out) {
    if (pure_translation(local)) {
        translate_world(p, local.position, out);
    } else {
        full_world(p, local, sc, out);
    }
}

} // namespace nnh::detail
