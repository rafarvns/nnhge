#pragma once
// Inline transform helpers for the particle loops (spec 024): the same formulas as spec 022's transform_point and
// transform_direction, and its mirror rule (a world rotation adds a child's, subtracted under a mirrored world).

#include <bit>
#include <cstdint>

#include "nnh/transform.hpp"

namespace nnh::detail {

inline Vec2 map_point(const Affine2D& m, Vec2 p) {
    return {(m.x.x * p.x + m.y.x * p.y) + m.origin.x, (m.x.y * p.x + m.y.y * p.y) + m.origin.y};
}

inline Vec2 map_direction(const Affine2D& m, Vec2 d) { return {m.x.x * d.x + m.y.x * d.y, m.x.y * d.x + m.y.y * d.y}; }

/// The sign bit set when a drawing scale mirrors (its components' signs differ).
inline std::uint32_t mirror_bit(Vec2 scale) {
    return (std::bit_cast<std::uint32_t>(scale.x) ^ std::bit_cast<std::uint32_t>(scale.y)) & 0x80000000u;
}

/// `v` with its sign flipped when `mirror` holds the sign bit.
inline float flip_sign(float v, std::uint32_t mirror) {
    return std::bit_cast<float>(std::bit_cast<std::uint32_t>(v) ^ mirror);
}

/// True when the map's axes are exactly (1, 0) and (0, 1): a pure translation.
inline bool translation_only(const Affine2D& m) {
    return std::bit_cast<std::uint32_t>(m.x.x) == 0x3F800000u && std::bit_cast<std::uint32_t>(m.y.y) == 0x3F800000u &&
           ((std::bit_cast<std::uint32_t>(m.x.y) | std::bit_cast<std::uint32_t>(m.y.x)) & 0x7FFFFFFFu) == 0u;
}

} // namespace nnh::detail
