// Misuse checks of the queries (spec 019 FR-17), see query_check.hpp.

#include "collision/query_check.hpp"

#include "collision/box_view.hpp"
#include "nnh/assert.hpp"

namespace nnh::collision::detail {

bool view_ok(const BoxView& v, const char* who) {
    const bool ok =
        v.count >= 0 &&
        (v.count == 0 || (v.first != nullptr && v.stride >= static_cast<int>(sizeof(Rect)) &&
                          (v.layers == nullptr || v.layers_stride >= static_cast<int>(sizeof(Layers)))));
    NNH_ASSERT(ok, "collision::%s: bad box view (count %d, stride %d, layers stride %d)", who, v.count,
               v.stride, v.layers_stride);
    (void)who;
    return ok;
}

bool buffer_ok(const void* out, int capacity, const char* who) {
    const bool ok = capacity >= 0 && (out != nullptr || capacity == 0);
    NNH_ASSERT(ok, "collision::%s: bad output buffer (capacity %d)", who, capacity);
    (void)who;
    return ok;
}

bool grid_ok(const Grid& grid, const char* who) {
    const bool ok = grid.cell_w > 0 && grid.cell_h > 0;
    NNH_ASSERT(ok, "collision::%s: cell size must be > 0", who);
    (void)who;
    return ok;
}

bool point_ok(Vec2 p, const char* who) {
    const bool ok = finite(p.x) && finite(p.y);
    NNH_ASSERT(ok, "collision::%s: point (%f, %f) not finite", who, static_cast<double>(p.x),
               static_cast<double>(p.y));
    (void)who;
    return ok;
}

bool rect_ok(Rect r, const char* who) {
    const bool ok = finite(r.x) && finite(r.y) && finite(r.w) && finite(r.h);
    NNH_ASSERT(ok, "collision::%s: box not finite", who);
    (void)who;
    return ok;
}

} // namespace nnh::collision::detail
