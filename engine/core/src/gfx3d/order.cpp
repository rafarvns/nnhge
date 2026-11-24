// The submission order (pass3d.hpp, FR-19): visible draws in call order for the opaque and cut-out parts, then the
// blended ones back to front by a bottom-up stable merge sort (ties keep call order). O(n log n), no allocation.

#include "gfx3d/pass3d.hpp"

namespace nnh::detail {

Order order_draws(const bool* visible, const bool* blended, const float* depth, int count, std::uint16_t* order,
                  std::uint16_t* scratch) {
    Order o;
    for (int i = 0; i < count; ++i)
        if (visible[i]) order[o.opaque++] = static_cast<std::uint16_t>(i);
    std::uint16_t* b = order + o.opaque;
    for (int i = 0; i < count; ++i)
        if (visible[i] && blended[i]) b[o.blended++] = static_cast<std::uint16_t>(i);
    const int n = o.blended;
    std::uint16_t* src = b;
    std::uint16_t* dst = scratch;
    for (int width = 1; width < n; width *= 2) {
        for (int lo = 0; lo < n; lo += 2 * width) {
            const int mid = lo + width < n ? lo + width : n, hi = lo + 2 * width < n ? lo + 2 * width : n;
            int i = lo, j = mid, k = lo;
            while (i < mid && j < hi) dst[k++] = depth[src[j]] < depth[src[i]] ? src[j++] : src[i++]; // farther first
            while (i < mid) dst[k++] = src[i++];
            while (j < hi) dst[k++] = src[j++];
        }
        std::uint16_t* t = src;
        src = dst;
        dst = t;
    }
    if (src != b)
        for (int i = 0; i < n; ++i) b[i] = src[i];
    return o;
}

} // namespace nnh::detail
