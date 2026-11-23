// Finding entities by tag (spec 020 FR-17): a scan of the records below the entity extent, one loop specialised per
// tags_per_entity (1..8) so the compares unroll. Destroyed and free records hold no tag (destroy zeroes them), so no
// liveness test is needed.

#include "entities/world_internal.hpp"

namespace nnh {
namespace detail {
namespace {

template <int N>
int scan(const WorldHot& w, std::uint32_t t, Entity* out, int capacity, bool first_only) {
    const std::uint32_t* r = w.records + 2;
    const int stride = w.record_words;
    const int extent = w.entities.extent;
    int total = 0;
    for (int slot = 0; slot < extent; ++slot, r += stride) {
        bool hit = false;
        for (int i = 0; i < N; ++i) hit = hit || r[i] == t;
        if (!hit) continue;
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
