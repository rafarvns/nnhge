// The world's all-or-nothing reservation (spec 021 T1.2, T1.3; entities-internal, for prefabs): the kind lookup by type
// id, and world_reserve() against spawn() + add<T>(e, value) + add_tag on twin worlds — the same handles, slots,
// bytes, tags and stats — plus each refusal, after which nothing but its counter has changed.

#include "entities/entities_internal.hpp"
#include "entities/world_internal.hpp"
#include "world_fixture.hpp"

#include <cstring>
#include <utility>

using nnh::Entity;
using nnh::Tag;
using nnh::World;
using nnh::detail::ReserveRefusal;
using world_test::Hp;
using world_test::Pos;
using world_test::Vel;

namespace {

template <int N>
struct K {
    int v = N;
};

template <int... N>
World make_32(std::integer_sequence<int, N...>) {
    const nnh::ComponentDesc kinds[] = {nnh::component<K<N>>(4)...};
    auto r = World::create({.max_entities = 4}, kinds);
    CHECK(r.ok());
    return static_cast<World&&>(r.value());
}

nnh::detail::WorldHot& hot(World& w) { return *nnh::WorldAccess::hot(w); }

/// Everything a twin must equal: every record word, the entity and kind tables (bitmaps, extents, counts, peaks,
/// misses), owners and component bytes (scratch slot included).
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

constexpr Tag kTags[4] = {nnh::tag("a"), nnh::tag("b"), nnh::tag("c"), nnh::tag("d")};

/// What a spawn made from the bits of `h` holds: kinds (any order), tags, values.
struct Order {
    int kinds[3];
    int count = 0;
    Tag tags[2];
    int tag_count = 0;
    int v = 0;
    explicit Order(std::uint32_t h) {
        for (int k = 0; k < 3; ++k)
            if ((h >> k) & 1u) kinds[count++] = k;
        if ((h >> 3) & 1u && count > 1) std::swap(kinds[0], kinds[count - 1]);
        tag_count = static_cast<int>((h >> 4) % 3);
        for (int i = 0; i < tag_count; ++i) tags[i] = kTags[(i + (h >> 6)) % 4];
        v = static_cast<int>(h >> 8);
    }
};

/// By reservation, then plain copies into the reserved slots.
Entity spawn_reserved(World& a, const Order& o) {
    void* slots[3];
    ReserveRefusal why{};
    int full = -1;
    const Entity e = nnh::detail::world_reserve(a, o.kinds, o.count, slots, o.tags, o.tag_count, why, &full);
    CHECK((e && why == ReserveRefusal::None) || (!e && why != ReserveRefusal::None));
    for (int i = 0; e && i < o.count; ++i) {
        void* p = slots[i];
        if (o.kinds[i] == 0) *static_cast<Pos*>(p) = {o.v, o.v + 1};
        if (o.kinds[i] == 1) *static_cast<Vel*>(p) = {o.v + 2, o.v + 3};
        if (o.kinds[i] == 2) *static_cast<Hp*>(p) = {o.v + 4};
    }
    return e;
}

/// By hand, all or nothing: the free room checked first through the same lowest-free query (a refusal counted where
/// spawn() or add() would have counted it).
Entity spawn_by_hand(World& b, const Order& o) {
    auto& y = hot(b);
    bool room = y.entities.find_free() >= 0;
    if (!room) ++y.entities.misses;
    for (int i = 0; room && i < o.count; ++i) {
        if (y.kinds[o.kinds[i]].slots.find_free() >= 0) continue;
        ++y.kinds[o.kinds[i]].slots.misses;
        room = false;
    }
    if (!room) return {};
    const Entity e = b.spawn();
    for (int i = 0; i < o.count; ++i) {
        if (o.kinds[i] == 0) b.add<Pos>(e, {o.v, o.v + 1});
        if (o.kinds[i] == 1) b.add<Vel>(e, {o.v + 2, o.v + 3});
        if (o.kinds[i] == 2) b.add<Hp>(e, {o.v + 4});
    }
    for (int i = 0; i < o.tag_count; ++i) b.add_tag(e, o.tags[i]);
    return e;
}

bool spawn_both(World& a, World& b, std::uint32_t h) {
    const Order o(h);
    return spawn_reserved(a, o) == spawn_by_hand(b, o);
}

std::uint32_t mix(std::uint32_t seed, std::uint32_t step) {
    const char bytes[8] = {static_cast<char>(seed), static_cast<char>(seed >> 8), static_cast<char>(step),
                           static_cast<char>(step >> 8), static_cast<char>(step >> 16), 'r', 's', 'v'};
    return nnh::detail::fnv1a32(bytes, sizeof bytes);
}

} // namespace

// ---- Part 1: kinds by type id ----

NNH_TEST(world_kind_of_finds_every_registered_kind_as_kind_index_does) {
    World w = world_test::make_world();
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<Pos>()) == 0);
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<Vel>()) == 1);
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<Hp>()) == 2);
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<K<0>>()) == -1);
    CHECK(nnh::detail::world_kind_of(World{}, nnh::detail::type_id<Pos>()) == -1);
    CHECK(nnh::detail::world_kind_of(w, nullptr) == -1);
}

NNH_TEST(world_kind_of_finds_kinds_past_the_fourth_of_thirty_two) {
    World w = make_32(std::make_integer_sequence<int, 32>{});
    const auto& h = hot(w);
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<K<5>>()) == 5);
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<K<31>>()) == 31);
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<K<3>>()) == nnh::detail::kind_index<K<3>>(h));
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<K<31>>()) == nnh::detail::kind_index<K<31>>(h));
    CHECK(nnh::detail::world_kind_of(w, nnh::detail::type_id<Pos>()) == -1);
}

NNH_TEST(world_component_points_at_a_kinds_slot) {
    World w = world_test::make_world();
    const Entity e = w.spawn();
    Vel* v = w.add<Vel>(e, {7, 8});
    CHECK(nnh::detail::world_component(w, 1, 0) == v);
    CHECK(nnh::detail::world_component(w, 1, 3) == v + 3);
}

// ---- Part 2: the reservation ----

NNH_TEST(world_reserve_equals_spawn_add_and_add_tag_on_twenty_seeded_sequences) {
    int reserved = 0, refused = 0;
    for (std::uint32_t seed = 1; seed <= 20; ++seed) {
        // Kinds smaller than the world, so full kinds happen; 2..4 tags per entity.
        const nnh::ComponentDesc kinds[] = {nnh::component<Pos>(48), nnh::component<Vel>(30), nnh::component<Hp>(12)};
        const nnh::WorldDesc desc{.max_entities = 48, .tags_per_entity = 2 + static_cast<int>(seed % 3)};
        World a = static_cast<World&&>(World::create(desc, kinds).value());
        World b = static_cast<World&&>(World::create(desc, kinds).value());
        Entity live[48] = {};
        bool same = true;
        for (std::uint32_t step = 0; step < 2000 && same; ++step) {
            const std::uint32_t h = mix(seed, step);
            const int op = static_cast<int>(h % 16);
            if (op < 8) {
                same = spawn_both(a, b, h >> 4);
                for (const Entity x : a.entities()) live[x.slot()] = x;
            } else if (op < 12) {
                const Entity e = live[(h >> 4) % 48];
                same = a.destroy(e) == b.destroy(e);
            } else if (op == 12) {
                a.flush();
                b.flush();
            } else if (op == 13) { // reserve / spawn inside an iteration: fresh, not visited
                const Order o(h >> 4);
                int visits_a = 0, visits_b = 0;
                Entity ea{}, eb{};
                a.each<Pos>([&](Entity, Pos&) {
                    if (visits_a++ == 0) ea = spawn_reserved(a, o);
                });
                b.each<Pos>([&](Entity, Pos&) {
                    if (visits_b++ == 0) eb = spawn_by_hand(b, o);
                });
                same = ea == eb && visits_a == visits_b;
            } else if (op == 14 && (h >> 4) % 8 == 0) {
                a.clear();
                b.clear();
            } else {
                const Entity e = live[(h >> 4) % 48];
                same = a.remove<Vel>(e) == b.remove<Vel>(e);
            }
            same = same && same_state(a, b);
        }
        CHECK(same);
        reserved += a.stats().peak;
        refused += a.stats().spawn_misses + a.storage<Hp>().stats().misses;
    }
    CHECK(reserved > 200 && refused > 20); // not vacuous: full kinds and full worlds were met
}

NNH_TEST(world_reserve_inside_a_remove_hook_is_allowed) {
    static World* world = nullptr;
    static Entity made{};
    const nnh::ComponentDesc kinds[] = {nnh::component<Pos>(8, [](World& w, Entity, Pos&) {
                                            if (w.clearing()) return; // the world's own end
                                            const int k[1] = {0};
                                            void* s[1];
                                            ReserveRefusal why{};
                                            made = nnh::detail::world_reserve(*world, k, 1, s, nullptr, 0, why, nullptr);
                                        }),
                                        nnh::component<Vel>(8)};
    World w = static_cast<World&&>(World::create({.max_entities = 8}, kinds).value());
    world = &w;
    const Entity e = w.spawn();
    w.add<Pos>(e);
    w.destroy(e);
    CHECK(w.alive(made) && made.slot() == 1 && w.has<Pos>(made));
    w.destroy(made); // its hook reserves again: slot 2
    CHECK(w.count() == 1);
    world = nullptr;
}

NNH_TEST(world_reserve_refuses_an_empty_world) {
    World w;
    const int k[1] = {0};
    void* s[1];
    ReserveRefusal why{};
    CHECK(!nnh::detail::world_reserve(w, k, 1, s, nullptr, 0, why, nullptr));
    CHECK(why == ReserveRefusal::Empty);
}

NNH_TEST(world_reserve_refuses_a_full_world_and_counts_a_spawn_miss) {
    World a = world_test::make_world(2, 2);
    World b = world_test::make_world(2, 2);
    for (World* w : {&a, &b}) {
        w->add<Pos>(w->spawn(), {1, 2});
        w->spawn();
    }
    const int k[2] = {0, 1};
    void* s[2];
    ReserveRefusal why{};
    int full = 99;
    CHECK(!nnh::detail::world_reserve(a, k, 2, s, nullptr, 0, why, &full));
    CHECK(why == ReserveRefusal::WorldFull && full == 99);
    CHECK(a.stats().spawn_misses == 1 && a.storage<Pos>().stats().misses == 0);
    ++hot(b).entities.misses;
    CHECK(same_state(a, b)); // nothing else changed
}

NNH_TEST(world_reserve_refuses_the_first_full_kind_and_takes_nothing) {
    const nnh::ComponentDesc kinds[] = {nnh::component<Pos>(4), nnh::component<Vel>(1), nnh::component<Hp>(1)};
    World a = static_cast<World&&>(World::create({.max_entities = 4}, kinds).value());
    World b = static_cast<World&&>(World::create({.max_entities = 4}, kinds).value());
    for (World* w : {&a, &b}) {
        const Entity e = w->spawn();
        w->add<Vel>(e);
        w->add<Hp>(e);
    }
    const int k[3] = {0, 2, 1}; // Hp (2) is the first full kind in this order
    void* s[3];
    ReserveRefusal why{};
    int full = -1;
    CHECK(!nnh::detail::world_reserve(a, k, 3, s, nullptr, 0, why, &full));
    CHECK(why == ReserveRefusal::KindFull && full == 2);
    CHECK(a.storage<Hp>().stats().misses == 1 && a.storage<Vel>().stats().misses == 0);
    CHECK(a.stats().spawn_misses == 0 && a.count() == 1);
    ++hot(b).kinds[2].slots.misses;
    CHECK(same_state(a, b));
}

NNH_TEST(world_reserve_inside_clear_refuses_without_counting) {
    static World* world = nullptr;
    static ReserveRefusal seen = ReserveRefusal::None;
    const nnh::ComponentDesc kinds[] = {nnh::component<Pos>(4, [](World&, Entity, Pos&) {
        const int k[1] = {0};
        void* s[1];
        if (nnh::detail::world_reserve(*world, k, 1, s, nullptr, 0, seen, nullptr)) seen = ReserveRefusal::None;
    })};
    World w = static_cast<World&&>(World::create({.max_entities = 4}, kinds).value());
    world = &w;
    w.add<Pos>(w.spawn());
    w.clear();
    CHECK(seen == ReserveRefusal::Clearing);
    CHECK(w.count() == 0 && w.stats().spawn_misses == 0 && w.storage<Pos>().stats().misses == 0);
    world = nullptr;
}

NNH_TEST(world_reserve_gives_each_components_address_and_reports_dropped_tags) {
    World w = world_test::make_world(4, 4, 1);
    const int k[2] = {1, 2};
    void* at[2];
    ReserveRefusal why{};
    const Entity e = nnh::detail::world_reserve(w, k, 2, at, kTags, 2, why, nullptr);
    CHECK(e && why == ReserveRefusal::TagsDropped);
    CHECK(at[0] == w.get<Vel>(e) && at[1] == w.get<Hp>(e) && w.has_tag(e, kTags[0]) && !w.has_tag(e, kTags[1]));
}

NNH_TEST(world_kinds_of_maps_a_strided_list_of_ids) {
    World w = world_test::make_world();
    struct Entry {
        nnh::detail::TypeId id;
        int pad[3];
    };
    const Entry list[3] = {{nnh::detail::type_id<Hp>(), {}}, {nnh::detail::type_id<Pos>(), {}},
                           {nnh::detail::type_id<K<1>>(), {}}};
    int out[3] = {-9, -9, -9};
    CHECK(nnh::detail::world_kinds_of(w, &list[0].id, sizeof(Entry), 2, out) == -1 && out[0] == 2 && out[1] == 0);
    CHECK(nnh::detail::world_kinds_of(w, &list[0].id, sizeof(Entry), 3, out) == 2);
    World big = make_32(std::make_integer_sequence<int, 32>{});
    const nnh::detail::TypeId ids[3] = {nnh::detail::type_id<K<30>>(), nnh::detail::type_id<K<2>>(), nullptr};
    CHECK(nnh::detail::world_kinds_of(big, ids, sizeof ids[0], 2, out) == -1 && out[0] == 30 && out[1] == 2);
    CHECK(nnh::detail::world_kinds_of(big, ids, sizeof ids[0], 3, out) == 2); // a null id is no kind
    CHECK(nnh::detail::world_kinds_of(World{}, ids, sizeof ids[0], 1, out) == 0);
    const nnh::detail::TypeId null_id[1] = {nullptr}; // mutation testing (plan 021 T8.1): 3 kinds, ids[3] is null too
    CHECK(nnh::detail::world_kinds_of(w, null_id, sizeof null_id[0], 1, out) == 0);
    CHECK(nnh::detail::world_kinds_of(World{}, ids, sizeof ids[0], 0, out) == -1);
}

NNH_TEST(world_reserve_writes_its_tags_in_order_and_no_kind_is_allowed) {
    World w = world_test::make_world(4, 4, 4);
    const Tag tags[3] = {kTags[2], kTags[0], kTags[3]};
    void* s[1];
    ReserveRefusal why{};
    const Entity e = nnh::detail::world_reserve(w, nullptr, 0, s, tags, 3, why, nullptr);
    CHECK(e && why == ReserveRefusal::None);
    const std::uint32_t* r = nnh::detail::record_at(hot(w), 0);
    CHECK(r[2] == static_cast<std::uint32_t>(kTags[2]) && r[3] == static_cast<std::uint32_t>(kTags[0]));
    CHECK(r[4] == static_cast<std::uint32_t>(kTags[3]) && r[5] == 0 && r[1] == 0);
    CHECK(w.has_tag(e, kTags[0]) && !w.has<Pos>(e));
}
