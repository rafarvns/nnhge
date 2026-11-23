#pragma once
// Misuse checks shared by the queries (spec 019 FR-17). Each asserts in Debug/Development (naming the query
// `who`) and returns false, so Release returns "no hits" without reading anything.

#include "nnh/collision.hpp"

namespace nnh::collision::detail {

/// Count ≥ 0; with boxes: `first` set, stride ≥ sizeof(Rect); with layers: layers_stride ≥ sizeof(Layers).
bool view_ok(const BoxView& view, const char* who);
/// Capacity ≥ 0 and a buffer when capacity > 0.
bool buffer_ok(const void* out, int capacity, const char* who);
/// Cell sizes > 0 (NaN fails).
bool grid_ok(const Grid& grid, const char* who);
/// Finite coordinates.
bool point_ok(Vec2 p, const char* who);
bool rect_ok(Rect r, const char* who);

} // namespace nnh::collision::detail
