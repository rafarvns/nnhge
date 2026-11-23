#pragma once
/// @file nnh/detail/slot_bits.hpp
/// Slot bookkeeping shared by pools and the component kinds of a world (spec 020, D35): a live bitmap, a quarantine
/// bitmap (released until the next flush), a fresh bitmap (taken during an iteration), the extent and the counters.
/// Everything inline and integer-only: the hot paths of <nnh/pool.hpp> and <nnh/entities.hpp> compile into the
/// game's own code (no call per slot, no division: words are `slot >> 5`, bits `slot & 31`). Engine-internal.

#include <cstdint>

namespace nnh::detail {

/// Generations go 1, 2, ..., 65 535, 1, ...: never 0, so a live handle is never null.
constexpr std::uint32_t next_generation(std::uint32_t generation) {
    return generation >= 0xFFFFu ? 1u : generation + 1u;
}

/// Index of the lowest set bit of `x` (x ≠ 0). ARMv6K has `clz` but no `rbit`: 31 − clz(x & −x).
inline int lowest_bit(std::uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return 31 - __builtin_clz(x & (0u - x));
#else
    int n = 0;
    while ((x & 1u) == 0) {
        x >>= 1;
        ++n;
    }
    return n;
#endif
}

/// Words of a bitmap of `capacity` bits.
constexpr int words_for(int capacity) { return (capacity + 31) >> 5; }

/// The bitmaps and counters of one table of slots. The bitmaps live elsewhere (inside a pool or a world's block);
/// slots at or past `capacity` are never taken: init_padding() sets their bits in the last quarantine word for good
/// (flush and reset keep them), so find_free needs no capacity compare. Invariant: every word below `hint` has no
/// free slot.
struct SlotTable {
    std::uint32_t* live = nullptr;
    std::uint32_t* quarantine = nullptr;
    std::uint32_t* fresh = nullptr;
    int capacity = 0;
    int extent = 0; ///< one past the highest slot taken since the last reset
    int count = 0;  ///< live slots
    int peak = 0;   ///< highest count since the last reset of the stats
    int misses = 0; ///< takes refused because the table was full
    int hint = 0;   ///< first word that may hold a free slot
    int words = 0;  ///< words_for(capacity), set by init_padding()
    std::uint32_t pad = 0; ///< the last word's bits past `capacity`, always set in `quarantine`

    /// After `capacity` is set and the bitmaps zeroed: the padding bits, the word count.
    void init_padding() {
        words = words_for(capacity);
        pad = (capacity & 31) != 0 ? ~0u << (capacity & 31) : 0u;
        if (words > 0) quarantine[words - 1] |= pad;
    }
    /// What quarantine word `w` holds when nothing in it is quarantined.
    std::uint32_t empty_quarantine(int w) const { return w == words - 1 ? pad : 0u; }

    bool is_live(int slot) const { return ((live[slot >> 5] >> (slot & 31)) & 1u) != 0; }

    /// The lowest slot neither live nor quarantined, or −1 when there is none.
    int find_free() {
        const int n = words;
        if (hint < n) { // the common case, straight-line: the hint word has room
            const std::uint32_t free = ~(live[hint] | quarantine[hint]);
            if (free != 0) return (hint << 5) + lowest_bit(free);
        }
        for (int w = hint + 1; w < n; ++w) {
            const std::uint32_t free = ~(live[w] | quarantine[w]); // padding bits read as taken
            if (free == 0) continue;
            if (w != hint) hint = w; // the common case (the hint word has room) stores nothing
            return (w << 5) + lowest_bit(free);
        }
        hint = n;
        return -1;
    }

    /// Marks `slot` (from find_free) live; during an iteration also fresh (iterations skip it).
    void take(int slot, bool iterating) {
        const std::uint32_t bit = 1u << (slot & 31);
        live[slot >> 5] |= bit;
        if (iterating) fresh[slot >> 5] |= bit;
        if (slot >= extent) extent = slot + 1;
        if (++count > peak) peak = count;
    }

    /// A live slot leaves: it stays out of reach (quarantined) until flush().
    void retire(int slot) {
        const std::uint32_t bit = 1u << (slot & 31);
        live[slot >> 5] &= ~bit;
        quarantine[slot >> 5] |= bit;
        --count;
    }

    /// Quarantined slots become free. Returns whether any was.
    bool flush() {
        const int n = words_for(extent);
        bool any = false;
        for (int w = 0; w < n; ++w) {
            const std::uint32_t empty = empty_quarantine(w);
            if (quarantine[w] == empty) continue;
            if (!any && w < hint) hint = w;
            any = true;
            quarantine[w] = empty;
        }
        return any;
    }

    /// The end of the outermost iteration: nothing is fresh any more.
    void end_iteration() {
        const int n = words_for(extent);
        for (int w = 0; w < n; ++w) fresh[w] = 0;
    }

    /// Every slot free, extent 0 (the caller has emptied the slots).
    void reset() {
        const int n = words_for(extent);
        for (int w = 0; w < n; ++w) {
            live[w] = 0;
            quarantine[w] = empty_quarantine(w);
            fresh[w] = 0;
        }
        extent = 0;
        count = 0;
        hint = 0;
    }
};

} // namespace nnh::detail
