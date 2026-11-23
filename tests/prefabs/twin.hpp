#pragma once
// Twin worlds (spec 021 test strategy): one world filled by prefab spawns, its twin by hand-written spawn/add/add_tag
// calls; same_state() compares everything a twin must equal.

#include <cstring>

#include "entities/world_internal.hpp"
#include "nnh/entities.hpp"

namespace prefab_test {

/// Every record word, the entity and kind tables (bitmaps, extents, counts, peaks, misses), owners and component
/// bytes (scratch slot included), and the pending word.
inline bool same_state(nnh::World& a, nnh::World& b) {
    const nnh::detail::WorldHot* x = nnh::WorldAccess::hot(a);
    const nnh::detail::WorldHot* y = nnh::WorldAccess::hot(b);
    if (x == nullptr || y == nullptr) return x == y;
    const auto table = [](const nnh::detail::SlotTable& s, const nnh::detail::SlotTable& t) {
        for (int i = 0; i < s.words; ++i)
            if (s.live[i] != t.live[i] || s.quarantine[i] != t.quarantine[i] || s.fresh[i] != t.fresh[i]) return false;
        return s.extent == t.extent && s.count == t.count && s.peak == t.peak && s.misses == t.misses;
    };
    if (!table(x->entities, y->entities) || x->pending != y->pending) return false;
    const std::size_t record_bytes = 4u * static_cast<std::size_t>(x->record_words * x->entities.capacity);
    if (std::memcmp(x->records, y->records, record_bytes) != 0) return false;
    for (int k = 0; k < x->kind_count; ++k) {
        const auto& p = x->kinds[k];
        const auto& q = y->kinds[k];
        if (!table(p.slots, q.slots)) return false;
        if (std::memcmp(p.owner, q.owner, 2u * static_cast<std::size_t>(p.slots.capacity)) != 0) return false;
        if (std::memcmp(p.data, q.data, static_cast<std::size_t>(p.size) * (p.slots.capacity + 1)) != 0) return false;
    }
    return true;
}

} // namespace prefab_test
