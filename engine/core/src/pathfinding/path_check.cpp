// Misuse reporting for pathfinding (spec 025 FR-23), see path_check.hpp.

#include "pathfinding/path_check.hpp"

#include "nnh/assert.hpp"

NNH_LOG_CHANNEL(path);

namespace nnh::detail {

void path_misuse(const char* what) {
    NNH_LOG_ERROR(path, "%s", what);
    NNH_ASSERT(false, "%s", what);
}

bool cell_ok(int width, int height, int x, int y, const char* who) {
    if (x >= 0 && y >= 0 && x < width && y < height) return true;
    NNH_LOG_ERROR(path, "%s: cell (%d, %d) outside the %d x %d map", who, x, y, width, height);
    NNH_ASSERT(false, "%s: cell (%d, %d) outside the %d x %d map", who, x, y, width, height);
    return false;
}

bool path_buffer_ok(const void* out, int capacity, const char* who) {
    if (capacity >= 0 && (out != nullptr || capacity == 0)) return true;
    NNH_LOG_ERROR(path, "%s: bad output buffer (capacity %d)", who, capacity);
    NNH_ASSERT(false, "%s: bad output buffer (capacity %d)", who, capacity);
    return false;
}

bool path_point_ok(Vec2 p, const char* who) {
    // Finite: not NaN (p == p) and not infinite (p - p == 0), without <cmath>.
    const bool ok = p.x == p.x && p.y == p.y && p.x - p.x == 0 && p.y - p.y == 0;
    if (ok) return true;
    NNH_LOG_ERROR(path, "%s: point not finite", who);
    NNH_ASSERT(false, "%s: point not finite", who);
    return false;
}

bool weight_ok(int weight, const char* who) {
    if (weight == kPathBlocked || (weight >= 1 && weight <= 255)) return true;
    NNH_LOG_ERROR(path, "%s: weight %d not 1..255 or kPathBlocked", who, weight);
    NNH_ASSERT(false, "%s: weight %d not 1..255 or kPathBlocked", who, weight);
    return false;
}

bool clip_region(CellRect r, int width, int height, CellSpan& span, const char* who) {
    span = {};
    if (r.w < 0 || r.h < 0) {
        NNH_LOG_ERROR(path, "%s: region size %d x %d is negative", who, r.w, r.h);
        NNH_ASSERT(false, "%s: region size %d x %d is negative", who, r.w, r.h);
        return false;
    }
    if (r.w == 0 || r.h == 0) return true;
    // In 64 bits: x + w may overflow int for huge values.
    const long long x1 = static_cast<long long>(r.x) + r.w - 1;
    const long long y1 = static_cast<long long>(r.y) + r.h - 1;
    if (x1 < 0 || y1 < 0 || r.x >= width || r.y >= height) {
        NNH_LOG_ERROR(path, "%s: region (%d, %d) %d x %d outside the %d x %d map", who, r.x, r.y, r.w, r.h, width,
                      height);
        NNH_ASSERT(false, "%s: region (%d, %d) %d x %d outside the %d x %d map", who, r.x, r.y, r.w, r.h, width,
                   height);
        return false;
    }
    span.x0 = r.x < 0 ? 0 : r.x;
    span.y0 = r.y < 0 ? 0 : r.y;
    span.x1 = x1 >= width ? width - 1 : static_cast<int>(x1);
    span.y1 = y1 >= height ? height - 1 : static_cast<int>(y1);
    return true;
}

} // namespace nnh::detail
