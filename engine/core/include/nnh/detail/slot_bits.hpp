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
/// slots at or past `capacity` are never taken. Invariant: every word below `hint` has no free slot.
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

    bool is_live(int slot) const { return ((live[slot >> 5] >> (slot & 31)) & 1u) != 0; }

    /// The lowest slot neither live nor quarantined, or −1 when there is none.
    int find_free() {
        const int words = words_for(capacity);
        for (int w = hint; w < words; ++w) {
            const std::uint32_t free = ~(live[w] | quarantine[w]);
            if (free == 0) continue;
            hint = w;
            const int slot = (w << 5) + lowest_bit(free);
            return slot < capacity ? slot : -1;
        }
        hint = words;
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
        const int words = words_for(extent);
        bool any = false;
        for (int w = 0; w < words; ++w) {
            if (quarantine[w] == 0) continue;
            if (!any && w < hint) hint = w;
            any = true;
            quarantine[w] = 0;
        }
        return any;
    }

    /// The end of the outermost iteration: nothing is fresh any more.
    void end_iteration() {
        const int words = words_for(extent);
        for (int w = 0; w < words; ++w) fresh[w] = 0;
    }

    /// Every slot free, extent 0 (the caller has emptied the slots).
    void reset() {
        const int words = words_for(extent);
        for (int w = 0; w < words; ++w) {
            live[w] = 0;
            quarantine[w] = 0;
            fresh[w] = 0;
        }
        extent = 0;
        count = 0;
        hint = 0;
    }
};

} // namespace nnh::detail
