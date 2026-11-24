#pragma once
// The A* open set (spec 025 FR-10, FR-12, D40): a binary heap of 16-byte entries ordered by the 64-bit key (f, h)
// ascending, then `seq` descending (the most recent push first), with lazy deletion (the searcher skips stale entries)
// and a one-entry buffer in front: of the children an expansion offers, the best is held aside and the next take()
// returns it if it beats the heap's top, else it replaces the top (one sift). The pop order is the total order either
// way: the buffer, compaction and the heap's layout never change a result. Push and pop move entries through a hole
// (one write per level, no swaps). Inline: the search calls them per neighbour.

#include <cstdint>

/// Hot-path helpers are forced inline (the T7.6 ARM pre-check: no out-of-line calls in the expansion loop).
#define NNH_PATH_INLINE inline __attribute__((always_inline))

namespace nnh::detail {

struct OpenEntry {
    std::uint32_t h = 0;    ///< the scaled heuristic
    std::uint32_t f = 0;    ///< g + h
    std::uint32_t seq = 0;  ///< the push's number in its search
    std::uint32_t cell = 0; ///< packed (y << 16 | x)
};

/// True when `a` pops before `b`: (f, h) as one 64-bit key, then the higher seq.
NNH_PATH_INLINE bool open_before(const OpenEntry& a, const OpenEntry& b) {
    const std::uint64_t ka = (static_cast<std::uint64_t>(a.f) << 32) | a.h;
    const std::uint64_t kb = (static_cast<std::uint64_t>(b.f) << 32) | b.h;
    return ka < kb || (ka == kb && a.seq > b.seq);
}

struct OpenHeap {
    OpenEntry* data = nullptr;
    int size = 0;
    int capacity = 0;
    OpenEntry held{};
    bool has_held = false;

    void reset() {
        size = 0;
        has_held = false;
    }
    bool full() const { return size >= capacity; }

    /// Into the heap (size < capacity).
    __attribute__((always_inline)) void push(const OpenEntry& e) {
        int i = size++;
        while (i > 0) {
            const int parent = (i - 1) >> 1;
            if (!open_before(e, data[parent])) break;
            data[i] = data[parent];
            i = parent;
        }
        data[i] = e;
    }

    /// Fills the hole at `i` with `e`, moving it down.
    __attribute__((always_inline)) void sift_down(int i, const OpenEntry& e) {
        const int n = size;
        while (true) {
            int c = 2 * i + 1;
            if (c >= n) break;
            if (c + 1 < n && open_before(data[c + 1], data[c])) ++c;
            if (!open_before(data[c], e)) break;
            data[i] = data[c];
            i = c;
        }
        data[i] = e;
    }

    /// A child of the current expansion: held aside if it is the best so far (the previous best goes into the heap),
    /// else into the heap. The caller makes room first (at most one entry enters the heap).
    __attribute__((always_inline)) void offer(const OpenEntry& e) {
        if (!has_held) {
            held = e;
            has_held = true;
        } else if (open_before(e, held)) {
            push(held);
            held = e;
        } else {
            push(e);
        }
    }

    /// The best entry (held or the heap's top); false when the set is empty.
    __attribute__((always_inline)) bool take(OpenEntry& out) {
        if (has_held) {
            has_held = false;
            if (size == 0 || open_before(held, data[0])) {
                out = held;
                return true;
            }
            out = data[0];
            sift_down(0, held); // the held entry replaces the top
            return true;
        }
        if (size == 0) return false;
        out = data[0];
        --size;
        if (size > 0) sift_down(0, data[size]);
        return true;
    }
};

/// Whether an entry is still worth popping (its cell open and its g current).
using OpenLive = bool (*)(const void* context, const OpenEntry& e);

/// Drops the heap entries `live` refuses and heapifies the rest (cold: only when a push finds the heap full). The held
/// entry is kept as it is.
void open_compact(OpenHeap& heap, OpenLive live, const void* context);

} // namespace nnh::detail
