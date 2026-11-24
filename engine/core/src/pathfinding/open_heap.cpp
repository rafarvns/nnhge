// Compaction of the open set (spec 025 D40), see open_heap.hpp. Cold: a heap with room for every cell of the map is
// full only when stale entries fill it, and live entries (at most one per open cell) are always fewer than the cells.

#include "pathfinding/open_heap.hpp"

namespace nnh::detail {

void open_compact(OpenHeap& heap, OpenLive live, const void* context) {
    int kept = 0;
    for (int i = 0; i < heap.size; ++i) {
        if (live(context, heap.data[i])) heap.data[kept++] = heap.data[i];
    }
    heap.size = kept;
    // Floyd's heapify: the order of equal keys never matters (seq makes every key unique).
    for (int i = (kept >> 1) - 1; i >= 0; --i) {
        const OpenEntry e = heap.data[i];
        heap.sift_down(i, e);
    }
}

} // namespace nnh::detail
