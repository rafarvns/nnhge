// The group reservation (spec 022 T2.2, FR-17; entities-internal, for prefab hierarchies): world_reserve_many(n)
// against n world_reserve() calls in a row on twin worlds — every record word, bitmap, counter, owner and component
// byte equal — on 20 seeded sequences; each refusal leaves the world as it was but its counter; inside a remove hook
// and an iteration (fresh: not visited).

#include "entities/entities_internal.hpp"
#include "entities/world_internal.hpp"
#include "world_fixture.hpp"

#include <cstring>

using nnh::Entity;
using nnh::Tag;
using nnh::World;
using nnh::detail::ReserveEntity;
using nnh::detail::ReserveRefusal;
using world_test::Hp;
using world_test::Pos;
using world_test::Vel;

namespace {

nnh::detail::WorldHot& hot(World& w) { return *nnh::WorldAccess::hot(w); }

bool same_state(World& a, World& b) {
    auto& x = hot(a);
    auto& y = hot(b);
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
        if (std::memcmp(p.data, q.data, static_cast<std::size_t>(p.size) * (p.slots.capacity + 1)) != 0) return false;
    }
    return true;
}

constexpr Tag kTags[3] = {nnh::tag("a"), nnh::tag("b"), nnh::tag("c")};

struct Rng {
    std::uint32_t s;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

/// A group of up to 32 entities with kinds and tags from the generator.
struct Group {
    ReserveEntity list[32];
    int kinds[96];
    int count = 0;
    explicit Group(Rng& rng, int max = 32) {
        count = 1 + static_cast<int>(rng.next() % static_cast<std::uint32_t>(max));
        int at = 0;
        for (int i = 0; i < count; ++i) {
            const std::uint32_t h = rng.next();
            list[i].first_kind = at;
            for (int k = 0; k < 3; ++k)
                if ((h >> k) & 1u) kinds[at++] = k;
            if ((h >> 3) & 1u && at - list[i].first_kind > 1) std::swap(kinds[list[i].first_kind], kinds[at - 1]);
            list[i].kind_count = at - list[i].first_kind;
            list[i].tags = kTags + ((h >> 4) % 2);
            list[i].tag_count = static_cast<int>((h >> 5) % 3);
        }
    }
};

/// The twin side: room checked first as world_reserve_many does (each world_reserve in turn, on a counting basis),
/// then `count` world_reserve() calls.
Entity reserve_one_by_one(World& b, const Group& g, Entity* out) {
    auto& y = hot(b);
    int need_e = 0, need[3] = {};
    int free_e = 0, free_k[3] = {};
    const auto count_free = [](const nnh::detail::SlotTable& t) {
        int n = 0;
        for (int i = 0; i < t.capacity; ++i)
            if (((t.live[i >> 5] | t.quarantine[i >> 5]) >> (i & 31) & 1u) == 0) ++n;
        return n;
    };
    free_e = count_free(y.entities);
    for (int k = 0; k < 3; ++k) free_k[k] = count_free(y.kinds[k].slots);
    for (int i = 0; i < g.count; ++i) {
        if (++need_e > free_e) {
            ++y.entities.misses;
            return {};
        }
        for (int j = 0; j < g.list[i].kind_count; ++j) {
            const int k = g.kinds[g.list[i].first_kind + j];
            if (++need[k] > free_k[k]) {
                ++y.kinds[k].slots.misses;
                return {};
            }
        }
    }
    for (int i = 0; i < g.count; ++i) {
        void* at[3];
        ReserveRefusal why{};
        out[i] = nnh::detail::world_reserve(b, g.kinds + g.list[i].first_kind, g.list[i].kind_count, at, g.list[i].tags,
                                            g.list[i].tag_count, why, nullptr);
        CHECK(out[i]);
    }
    return out[0];
}

World make(int entities, int cap) {
    const nnh::ComponentDesc kinds[] = {nnh::component<Pos>(cap), nnh::component<Vel>(cap), nnh::component<Hp>(cap)};
    auto r = World::create({.max_entities = entities, .tags_per_entity = 2}, kinds);
    CHECK(r.ok());
    return static_cast<World&&>(r.value());
}

} // namespace

NNH_TEST(world_reserve_many_equals_one_by_one_on_twenty_seeded_sequences) {
    for (std::uint32_t seed = 1; seed <= 20; ++seed) {
        World a = make(96, 64), b = make(96, 64);
        Rng rng{seed * 2654435761u};
        int refusals = 0;
        for (int step = 0; step < 2000; ++step) {
            const std::uint32_t op = rng.next() % 8;
            if (op <= 2) {
                const Group g(rng, op == 0 ? 32 : 6);
                Entity out_a[32], out_b[32];
                void* comps[96];
                ReserveRefusal why{};
                const Entity ea = nnh::detail::world_reserve_many(a, g.list, g.count, g.kinds, out_a, comps, why, nullptr);
                const Entity eb = reserve_one_by_one(b, g, out_b);
                CHECK(ea == eb);
                for (int i = 0; ea && i < g.count; ++i) CHECK(out_a[i] == out_b[i]);
                if (!ea) ++refusals;
            } else if (op <= 5) {
                const Entity e = nnh::detail::entity_at(hot(a), static_cast<int>(rng.next() % 96));
                CHECK(a.destroy(e) == b.destroy(e));
            } else if (op == 6) {
                a.flush();
                b.flush();
            } else if (hot(a).entities.find_free() >= 0 && hot(b).entities.find_free() >= 0) {
                const Entity e = a.spawn();
                CHECK(e == b.spawn());
                if (e && hot(a).kinds[2].slots.find_free() >= 0 && hot(b).kinds[2].slots.find_free() >= 0) {
                    a.add<Hp>(e, {7});
                    b.add<Hp>(e, {7});
                }
            }
            if ((step & 63) == 0) CHECK(same_state(a, b));
        }
        CHECK(same_state(a, b));
        CHECK(refusals > 0); // the sequences reach full capacities
    }
}

NNH_TEST(world_reserve_many_writes_components_in_list_order) {
    World w = make(8, 8);
    ReserveEntity list[2] = {{0, 2, kTags, 1}, {2, 1, nullptr, 0}};
    const int kinds[3] = {2, 0, 1};
    Entity out[2];
    void* comps[3];
    ReserveRefusal why{};
    CHECK(nnh::detail::world_reserve_many(w, list, 2, kinds, out, comps, why, nullptr) == out[0]);
    CHECK(why == ReserveRefusal::None && out[0].slot() == 0 && out[1].slot() == 1);
    CHECK(comps[0] == w.get<Hp>(out[0]) && comps[1] == w.get<Pos>(out[0]) && comps[2] == w.get<Vel>(out[1]));
    CHECK(w.has_tag(out[0], kTags[0]) && !w.has<Pos>(out[1]));
}

NNH_TEST(world_reserve_many_refusals_change_nothing_but_the_counter) {
    World a = make(4, 2), b = make(4, 2);
    ReserveEntity list[3] = {{0, 1}, {1, 1}, {2, 1}};
    const int kinds[3] = {0, 1, 0}; // Pos twice: the third entity finds Pos full
    Entity out[3];
    void* comps[3];
    ReserveRefusal why{};
    int full = -1;
    a.add<Pos>(a.spawn());
    b.add<Pos>(b.spawn());
    CHECK(!nnh::detail::world_reserve_many(a, list, 3, kinds, out, comps, why, &full));
    CHECK(why == ReserveRefusal::KindFull && full == 0);
    ++hot(b).kinds[0].slots.misses;
    CHECK(same_state(a, b));
    // Five entities into a world of four with one live: WorldFull on the fourth.
    ReserveEntity five[5] = {};
    CHECK(!nnh::detail::world_reserve_many(a, five, 4, kinds, out, comps, why, &full));
    CHECK(why == ReserveRefusal::WorldFull);
    ++hot(b).entities.misses;
    CHECK(same_state(a, b));
    World empty;
    CHECK(!nnh::detail::world_reserve_many(empty, list, 1, kinds, out, comps, why, nullptr) && why == ReserveRefusal::Empty);
}

NNH_TEST(world_reserve_many_inside_a_hook_and_an_iteration) {
    static Entity made[2];
    const nnh::ComponentDesc kinds[] = {nnh::component<Pos>(8, [](World& w, Entity, Pos&) {
                                            ReserveEntity list[2] = {{0, 1}, {1, 1}};
                                            const int k[2] = {1, 1};
                                            void* comps[2];
                                            ReserveRefusal why{};
                                            nnh::detail::world_reserve_many(w, list, 2, k, made, comps, why, nullptr);
                                        }),
                                        nnh::component<Vel>(8)};
    auto r = World::create({.max_entities = 8}, kinds);
    World w = static_cast<World&&>(r.value());
    const Entity e = w.spawn();
    w.add<Pos>(e);
    w.destroy(e);
    CHECK(w.alive(made[0]) && w.alive(made[1]) && w.has<Vel>(made[1]));
    int visited = 0;
    w.each<Vel>([&](Entity, Vel&) {
        ++visited;
        ReserveEntity list[1] = {{0, 1}};
        const int k[1] = {1};
        Entity out[1];
        void* comps[1];
        ReserveRefusal why{};
        CHECK(nnh::detail::world_reserve_many(w, list, 1, k, out, comps, why, nullptr));
    });
    CHECK(visited == 2 && w.storage<Vel>().count() == 4); // the fresh ones were not visited
}

NNH_TEST(world_reserve_many_reports_dropped_tags) {
    World w = make(8, 8); // two tags per entity
    const Tag three[3] = {kTags[0], kTags[1], kTags[2]};
    ReserveEntity list[2] = {{0, 0, three, 2}, {0, 0, three, 3}};
    const int kinds[1] = {0};
    Entity out[2];
    void* comps[1];
    ReserveRefusal why{};
    CHECK(nnh::detail::world_reserve_many(w, list, 2, kinds, out, comps, why, nullptr));
    CHECK(why == ReserveRefusal::TagsDropped && w.has_tag(out[1], kTags[1]) && !w.has_tag(out[1], kTags[2]));
    CHECK(nnh::detail::world_reserve_many(w, list, 1, kinds, out, comps, why, nullptr) && why == ReserveRefusal::None);
}
