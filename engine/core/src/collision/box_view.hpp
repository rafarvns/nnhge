#pragma once
// Reading a BoxView (spec 019 FR-3, FR-4): box i and its layer set by memcpy (no aliasing assumption about
// the game's struct; compiles to plain loads), and whether a box is a candidate for a query. Internal, pure.

#include "nnh/collision.hpp"

#include <cstddef>
#include <cstring>

namespace nnh::collision::detail {

inline Rect box_at(const BoxView& v, int i) {
    Rect r;
    std::memcpy(&r, static_cast<const char*>(v.first) + static_cast<std::ptrdiff_t>(i) * v.stride, sizeof r);
    return r;
}

/// kAllLayers when the view has no layers.
inline Layers layers_at(const BoxView& v, int i) {
    if (v.layers == nullptr) return kAllLayers;
    Layers l;
    std::memcpy(&l, static_cast<const char*>(v.layers) + static_cast<std::ptrdiff_t>(i) * v.layers_stride,
                sizeof l);
    return l;
}

/// True when `v` is finite (NaN and infinities fail; no <cmath> needed).
inline bool finite(float v) {
    return v - v == 0;
}

/// Box i is a candidate: detected by filter.mask, not filter.ignore, finite, with area. Reads it into `box`.
inline bool passes(const BoxView& v, const QueryFilter& filter, int i, Rect& box) {
    if (i == filter.ignore || !detects(filter.mask, layers_at(v, i))) return false;
    box = box_at(v, i);
    return box.w > 0 && box.h > 0 && finite(box.x) && finite(box.y) && finite(box.w) && finite(box.h);
}

} // namespace nnh::collision::detail
