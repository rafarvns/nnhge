#pragma once
// Debug drawing of queries with gfx (spec 019: not in the collision module, which depends only on math). Short
// and copyable: a ray, the cells it crosses, hits with their normals, a box at a cast's contact.

#include <nnh/collision.hpp>
#include <nnh/gfx.hpp>

namespace query_draw {

/// The segment from → to.
void ray(nnh::Screen screen, nnh::Vec2 from, nnh::Vec2 to, nnh::Color color, int layer);
/// Shades each crossed cell of `grid` (blocking kinds darker).
void cells(nnh::Screen screen, const nnh::collision::Grid& grid, const nnh::collision::CellCrossing* cells, int count,
           int layer);
/// A hit: a small square on its point and its normal as an 8-pixel line; `first` bigger and yellow.
void hit(nnh::Screen screen, const nnh::collision::RayHit& hit, bool first, int layer);
/// The outline of a box of `size` placed at a cast's contact.
void cast_box(nnh::Screen screen, nnh::Vec2 top_left, nnh::Vec2 size, nnh::Color color, int layer);

} // namespace query_draw
