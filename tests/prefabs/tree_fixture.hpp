#pragma once
// Shared helpers for the prefab hierarchy tests (spec 022 stage 5): worlds with the node kind, libraries with child
// capacity, a hash of a library's whole block (child entries and tree facts included) and a twin-world comparison of
// every record, table, owner and component byte (nodes included).

#include <cstring>

#include "hierarchy/hierarchy_internal.hpp"
#include "nnh/hierarchy.hpp"
#include "prefab_fixture.hpp"
#include "prefabs/library_layout.hpp"

namespace tree_test {

using namespace prefab_test;

/// Node, Body, Coin, Big, Mover, Bob, Counted (`cap` each) and `tags` tags per entity.
inline nnh::World make_tree_world(int entities = 64, int cap = 64, int tags = 4) {
    const nnh::ComponentDesc kinds[] = {nnh::node_component(cap), nnh::component<Body>(cap), nnh::component<Coin>(cap),
                                        nnh::component<Big>(cap),     nnh::component<Mover>(cap),
                                        nnh::component<Bob>(cap),     nnh::component<Counted>(cap)};
    auto r = nnh::World::create({.max_entities = entities, .tags_per_entity = tags}, kinds);
    CHECK(r.ok());
    return static_cast<nnh::World&&>(r.value());
}

inline nnh::PrefabLibrary make_tree_library(int max_children = 32, nnh::WhenFull when_full = nnh::WhenFull::Assert) {
    return make_library({.max_prefabs = 48, .max_entries = 256, .value_bytes = 8192, .when_full = when_full,
                         .max_children = max_children});
}

/// FNV-1a of a library's whole block: header, tables, records, entries, values, child entries and facts.
inline std::uint32_t whole_block_hash(const nnh::PrefabLibrary& lib) {
    const nnh::detail::PrefabBlock* b = nnh::PrefabAccess::block(lib);
    if (b == nullptr) return 0;
    const auto* bytes = reinterpret_cast<const unsigned char*>(b);
    const unsigned char* end = b->values + b->value_capacity;
    if (b->facts != nullptr) end = reinterpret_cast<const unsigned char*>(b->facts + b->max_prefabs);
    std::uint32_t h = 2166136261u;
    for (const unsigned char* p = bytes; p < end; ++p) h = (h ^ *p) * 16777619u;
    return h;
}

/// Every record word, the entity and kind tables (bitmaps, extents, counts, peaks, misses), owners and component bytes.
inline bool same_state(nnh::World& a, nnh::World& b) {
    auto& x = *nnh::WorldAccess::hot(a);
    auto& y = *nnh::WorldAccess::hot(b);
    const auto table = [](const nnh::detail::SlotTable& s, const nnh::detail::SlotTable& t) {
        for (int i = 0; i < s.words; ++i)
            if (s.live[i] != t.live[i] || s.quarantine[i] != t.quarantine[i] || s.fresh[i] != t.fresh[i]) return false;
        return s.extent == t.extent && s.count == t.count && s.peak == t.peak && s.misses == t.misses;
    };
    if (!table(x.entities, y.entities) || x.pending != y.pending) return false;
    const std::size_t record_bytes = 4u * static_cast<std::size_t>(x.record_words * x.entities.capacity);
    if (std::memcmp(x.records, y.records, record_bytes) != 0) return false;
    for (int k = 0; k < x.kind_count; ++k) {
        const auto& p = x.kinds[k];
        const auto& q = y.kinds[k];
        if (!table(p.slots, q.slots)) return false;
        if (std::memcmp(p.owner, q.owner, 2u * static_cast<std::size_t>(p.slots.capacity)) != 0) return false;
        if (std::memcmp(p.data, q.data, static_cast<std::size_t>(p.size) * p.slots.capacity) != 0) return false;
    }
    return true;
}

inline nnh::Node node_at(float x, float y, float rotation = 0) {
    return nnh::Node(nnh::Transform2D{.position = {x, y}, .rotation = rotation});
}

} // namespace tree_test
