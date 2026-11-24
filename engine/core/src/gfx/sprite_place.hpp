#pragma once
// Where a 2D item lands on a screen (spec 004, D33; shared by DrawList::build and the particle expansion, spec 024
// D39): positions minus the camera, times the zoom, rounded to whole pixels; sizes and pivots times the zoom. One rule,
// so a particle is drawn exactly as the gfx::draw (or fill_rect) of its frame would be. Included only by
// -ffp-contract=off sources (the zoom list in engine/core/CMakeLists.txt).

#include <cmath>

#include "nnh/math.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::detail {

/// Whole pixels, halves rounded up (the same on every system, so PC and 3DS agree).
inline float snap(float v) { return std::floor(v + 0.5f); }

/// A sprite's item from its resolved region (never empty), flips, scale, pivot and rotation; `at` = position − camera,
/// `z` = zoom (1 when ignoring the camera). A negative scale mirrors the sprite: the same as a flip with a positive scale.
inline void place_sprite(Region region, bool flip_x, bool flip_y, Vec2 scale, Vec2 pivot, float rotation, Vec2 at,
                         float z, platform::DrawItem2D& item) {
    item.region = region;
    item.flip_x = flip_x != (scale.x < 0);
    item.flip_y = flip_y != (scale.y < 0);
    const float sx = std::fabs(scale.x);
    const float sy = std::fabs(scale.y);
    item.w = static_cast<float>(item.region.w) * sx * z;
    item.h = static_cast<float>(item.region.h) * sy * z;
    item.pivot_x = pivot.x * sx * z;
    item.pivot_y = pivot.y * sy * z;
    item.rotation = rotation;
    item.x = snap(at.x * z);
    item.y = snap(at.y * z);
}

/// A filled or outlined rectangle's item: `at` = top-left − camera, `size` its size. Both corners are rounded, so
/// touching rectangles stay touching.
inline void place_rect(Vec2 at, Vec2 size, float z, platform::DrawItem2D& item) {
    item.x = snap(at.x * z);
    item.y = snap(at.y * z);
    item.w = snap((at.x + size.x) * z) - item.x;
    item.h = snap((at.y + size.y) * z) - item.y;
}

} // namespace nnh::detail
