// Finding entities by tag (spec 020 FR-17): a scan of the records below the entity extent, one loop specialised per
// tags_per_entity (1..8) so the compares unroll. Destroyed and free records hold no tag (destroy zeroes them), so no
// liveness test is needed.

#include "entities/world_internal.hpp"

#include <utility>

namespace nnh {
namespace detail {
namespace {

/// Whether one of a record's N tag words is `t`: N compares unrolled, no branch per word (plan 020 ARM pre-check: a
/// short-circuit loop here cost about 35 instructions per record on the ARM11).
template <std::size_t... I>
inline bool tagged(const std::uint32_t* r, std::uint32_t t, std::index_sequence<I...>) {
    return ((r[I] == t) || ...);
}

constexpr int kPrefetchAhead = 6; // ≈ 170 bytes at 4 tags: 5 to 6 cache lines of 32 bytes

inline void prefetch(const std::uint32_t* p) {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(p);
#else
    (void)p;
#endif
}

template <int N>
int scan(const WorldHot& w, std::uint32_t t, Entity* out, int capacity, bool first_only) {
    const std::uint32_t* r = w.records + 2;
    const int stride = w.record_words;
    const int extent = w.entities.extent;
    int total = 0;
    // The walk is sequential and the ARM11 has no hardware prefetcher (and the Old 3DS no L2): ask for the record
    // kPrefetchAhead records ahead with PLD (a hint: it never faults); stop asking near the end.
    const int prefetch_end = extent - kPrefetchAhead;
    for (int slot = 0; slot < extent; ++slot, r += stride) {
        if (slot < prefetch_end) prefetch(r + kPrefetchAhead * stride);
        if (!tagged(r, t, std::make_index_sequence<N>{})) continue;
        if (total < capacity) out[total] = entity_at(w, slot);
        ++total;
        if (first_only) break;
    }
    return total;
}

using Scan = int (*)(const WorldHot&, std::uint32_t, Entity*, int, bool);
constexpr Scan kScans[] = {nullptr, &scan<1>, &scan<2>, &scan<3>, &scan<4>, &scan<5>, &scan<6>, &scan<7>, &scan<8>};

int find_in(const WorldHot* w, Tag t, Entity* out, int capacity, bool first_only) {
    if (w == nullptr) {
        world_misuse("use of an empty or moved-from nnh::World");
        return 0;
    }
    if (capacity < 0 || (out == nullptr && capacity > 0)) {
        world_misuse("World::find(): a negative capacity, or a null buffer with capacity > 0");
        return 0;
    }
    if (w->tags == 0 || t == Tag::None) return 0;
    return kScans[w->tags](*w, static_cast<std::uint32_t>(t), out, capacity, first_only);
}

} // namespace
} // namespace detail

int World::find(Tag t, Entity* out, int capacity) const { return detail::find_in(hot_, t, out, capacity, false); }

Entity World::first(Tag t) const {
    Entity e{};
    detail::find_in(hot_, t, &e, 1, true);
    return e;
}

} // namespace nnh
