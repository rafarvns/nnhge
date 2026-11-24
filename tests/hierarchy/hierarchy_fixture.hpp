#pragma once
// Shared helpers for the hierarchy tests (spec 022 stages 3–4): kinds, a world maker, node spawns, a brute-force check
// of every link invariant, a hash of the node storage and module words (refusals change nothing), bit comparisons.

#include <bit>
#include <cstdint>
#include <cstring>

#include "../entities/world_fixture.hpp"
#include "hierarchy/hierarchy_internal.hpp"
#include "nnh/hierarchy.hpp"
#include "test.hpp"

namespace hierarchy_test {

struct Body {
    nnh::Rect box{};
    std::uint32_t layers = 0;
};
struct Mark {
    int value = 0;
};

/// A world with nodes (`nodes` capacity), Body and Mark kinds.
inline nnh::World make_world(int entities = 64, int nodes = 64) {
    const nnh::ComponentDesc kinds[] = {nnh::node_component(nodes), nnh::component<Body>(entities),
                                        nnh::component<Mark>(entities)};
    auto r = nnh::World::create({.max_entities = entities, .tags_per_entity = 2}, kinds);
    CHECK(r.ok());
    return static_cast<nnh::World&&>(r.value());
}

/// A new entity with a node at `local`.
inline nnh::Entity spawn_node(nnh::World& w, const nnh::Transform2D& local = {}) {
    const nnh::Entity e = w.spawn();
    w.add<nnh::Node>(e, nnh::Node(local));
    return e;
}

inline std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f); }

/// Every float of two world transforms has the same bits.
inline bool same_bits(const nnh::WorldTransform2D& a, const nnh::WorldTransform2D& b) {
    return std::memcmp(&a, &b, sizeof a) == 0;
}

/// FNV-1a of the node array (every slot and the scratch), the module words and the entity records.
inline std::uint32_t state_hash(nnh::World& world) {
    nnh::detail::WorldHot* w = nnh::WorldAccess::hot(world);
    const int kind = nnh::detail::world_kind_of(world, nnh::detail::type_id<nnh::Node>());
    std::uint32_t h = 2166136261u;
    const auto mix = [&h](const void* p, std::size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 16777619u;
    };
    mix(w->kinds[kind].data, sizeof(nnh::Node) * static_cast<std::size_t>(w->kinds[kind].slots.capacity + 1));
    mix(w->cold->modules.words, sizeof w->cold->modules.words);
    mix(w->records, 4u * static_cast<std::size_t>(w->record_words * w->entities.capacity));
    return h;
}

/// Checks every link invariant by brute force: parents live, each child listed once under its parent in a consistent
/// doubly linked list (the first's prev = the last), counts, depths. Returns the number of live nodes checked.
inline int check_links(nnh::World& world) {
    using nnh::detail::kNoNode;
    using nnh::detail::NA;
    nnh::Hierarchy h(world);
    const nnh::detail::Tree t = nnh::detail::tree_of(h);
    nnh::Storage<nnh::Node> store = world.storage<nnh::Node>();
    int checked = 0;
    for (int s = 0; s < store.extent(); ++s) {
        if (!store.is_live(s)) continue;
        ++checked;
        const nnh::Node& n = t.nodes[s];
        const std::uint16_t p = NA::parent(n);
        if (p == kNoNode) {
            CHECK(NA::depth(n) == 0 && NA::next(n) == kNoNode && NA::prev(n) == kNoNode);
        } else {
            CHECK(store.is_live(p) && NA::depth(n) == NA::depth(t.nodes[p]) + 1);
            int found = 0;
            for (std::uint16_t c = NA::first(t.nodes[p]); c != kNoNode; c = NA::next(t.nodes[c])) found += c == s;
            CHECK(found == 1);
        }
        int count = 0;
        std::uint16_t last = kNoNode;
        for (std::uint16_t c = NA::first(n); c != kNoNode; c = NA::next(t.nodes[c])) {
            CHECK(NA::parent(t.nodes[c]) == s);
            if (last != kNoNode) CHECK(NA::prev(t.nodes[c]) == last);
            last = c;
            ++count;
            CHECK(count <= 1024);
            if (count > 1024) break;
        }
        CHECK(count == NA::children(n));
        if (last != kNoNode) CHECK(NA::prev(t.nodes[NA::first(n)]) == last);
        CHECK(NA::depth(n) < nnh::kMaxHierarchyDepth);
    }
    return checked;
}

using world_test::asserts_or_is_ignored;
using world_test::crashes;

} // namespace hierarchy_test
