#pragma once
// The distance field's open set (spec 025 FR-16, D40; T10.3): a 4-ary min-heap of 64-bit keys (d << 32 | padded index),
// lazily deleted. Keys are unique (a cell is pushed only with a strictly lower d, a goal once), so the pop order is the
// sorted order whatever the heap's shape: arity and layout change no result. A node's four children share one 32-byte
// line (the array starts 8 bytes before a line, see field_create.cpp); one 64-bit compare orders two entries. Inline:
// the build calls them per cell.

#include <cstdint>

#include "pathfinding/open_heap.hpp" // NNH_PATH_INLINE

namespace nnh::detail {

using FieldKey = std::uint64_t;

NNH_PATH_INLINE FieldKey field_key(std::uint32_t d, std::uint32_t cell) {
    return (FieldKey{d} << 32) | cell;
}
NNH_PATH_INLINE std::uint32_t field_key_d(FieldKey k) { return static_cast<std::uint32_t>(k >> 32); }
NNH_PATH_INLINE std::uint32_t field_key_cell(FieldKey k) { return static_cast<std::uint32_t>(k); }

/// Puts `k` in the hole at `i`, moving it up.
NNH_PATH_INLINE void field_sift_up(FieldKey* h, int i, FieldKey k) {
    while (i > 0) {
        const int parent = (i - 1) >> 2;
        const FieldKey p = h[parent];
        if (p < k) break;
        h[i] = p;
        i = parent;
    }
    h[i] = k;
}

/// Puts `k` in the hole at `i` of a heap of `n` entries, moving it down.
NNH_PATH_INLINE void field_sift_down(FieldKey* h, int n, int i, FieldKey k) {
    while (true) {
        const int c = 4 * i + 1;
        if (c >= n) break;
        int best = c;
        FieldKey bk = h[c];
        if (c + 3 < n) { // four children: the common case, unrolled
            const FieldKey k1 = h[c + 1];
            const FieldKey k2 = h[c + 2];
            const FieldKey k3 = h[c + 3];
            if (k1 < bk) bk = k1, best = c + 1;
            if (k2 < bk) bk = k2, best = c + 2;
            if (k3 < bk) bk = k3, best = c + 3;
        } else {
            for (int j = c + 1; j < n; ++j) {
                if (h[j] < bk) bk = h[j], best = j;
            }
        }
        if (k < bk) break;
        h[i] = bk;
        i = best;
    }
    h[i] = k;
}

/// Removes the top of a heap of `n` entries (n >= 1; the caller read it): the hole walks down to a leaf along
/// the smallest children, then the last entry fills it, moving up (it came from the bottom, so it rarely moves
/// far). Fewer compares than a sift-down of the last entry; the same heap property, so the same pops.
NNH_PATH_INLINE void field_pop(FieldKey* h, int n) {
    const FieldKey last = h[--n];
    int i = 0;
    while (true) {
        const int c = 4 * i + 1;
        if (c + 3 < n) { // four children: the common case, unrolled
            int best = c;
            FieldKey bk = h[c];
            const FieldKey k1 = h[c + 1];
            const FieldKey k2 = h[c + 2];
            const FieldKey k3 = h[c + 3];
            if (k1 < bk) bk = k1, best = c + 1;
            if (k2 < bk) bk = k2, best = c + 2;
            if (k3 < bk) bk = k3, best = c + 3;
            h[i] = bk;
            i = best;
            continue;
        }
        if (c < n) {
            int best = c;
            FieldKey bk = h[c];
            for (int j = c + 1; j < n; ++j) {
                if (h[j] < bk) bk = h[j], best = j;
            }
            h[i] = bk;
            i = best;
        }
        break;
    }
    field_sift_up(h, i, last);
}

/// Floyd's heapify of `n` entries (cold: after a compaction).
inline void field_heapify(FieldKey* h, int n) {
    for (int i = (n - 2) >> 2; i >= 0; --i) field_sift_down(h, n, i, h[i]);
}

} // namespace nnh::detail
