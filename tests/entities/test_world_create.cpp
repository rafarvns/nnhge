// Creating worlds (spec 020 FR-9, FR-10, FR-23, FR-24; T2.2): every InvalidArgument case, OutOfMemory, one block of
// the `entities` tag (or the game's), T{} in every slot, destructor, move, empty and moved-from worlds.

#include "alloc_counter.hpp"
#include "entities/block_layout.hpp"
#include "entities/world_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/memory.hpp"
#include "world_fixture.hpp"

using nnh::ComponentDesc;
using nnh::ErrorCode;
using nnh::World;
using world_test::Counted;
using world_test::Hp;
using world_test::Pos;
using world_test::Vel;

namespace {

ErrorCode error_of(const nnh::WorldDesc& desc, const ComponentDesc* kinds, int count) {
    auto r = World::create(desc, kinds, count);
    return r.ok() ? ErrorCode::None : r.error().code;
}

/// An animator-like kind: not copyable, not movable.
struct Pinned {
    Pinned() = default;
    Pinned(const Pinned&) = delete;
    Pinned& operator=(const Pinned&) = delete;
    int frame = 3;
};

} // namespace

NNH_TEST(world_create_refuses_bad_descriptions) {
    const ComponentDesc one[] = {nnh::component<Pos>(4)};
    const ErrorCode bad = ErrorCode::InvalidArgument;
    CHECK(error_of({.max_entities = 0}, one, 1) == bad);
    CHECK(error_of({.max_entities = 65536}, one, 1) == bad);
    CHECK(error_of({.max_entities = 4, .tags_per_entity = 9}, one, 1) == bad);
    CHECK(error_of({.max_entities = 4, .tags_per_entity = -1}, one, 1) == bad);
    CHECK(error_of({.max_entities = 4}, one, 0) == bad); // no kind
    const ComponentDesc twice[] = {nnh::component<Pos>(4), nnh::component<Pos>(4)};
    CHECK(error_of({.max_entities = 4}, twice, 2) == bad);
    const ComponentDesc zero[] = {nnh::component<Pos>(0)};
    CHECK(error_of({.max_entities = 4}, zero, 1) == bad);
    const ComponentDesc over[] = {nnh::component<Pos>(5)};
    CHECK(error_of({.max_entities = 4}, over, 1) == bad);
    CHECK(error_of({.max_entities = 4}, nullptr, 1) == bad);
    CHECK(error_of({.max_entities = 65535}, one, 1) == ErrorCode::None);
    CHECK(error_of({.max_entities = 4, .tags_per_entity = 8}, one, 1) == ErrorCode::None);
    CHECK(error_of({.max_entities = 4, .tags_per_entity = 0}, one, 1) == ErrorCode::None);
}

NNH_TEST(world_create_accepts_32_kinds_and_refuses_33) {
    struct K {};
    ComponentDesc many[33];
    // 33 distinct type ids are hard to spell; distinct ids are what matters, so forge them from one array.
    static const char ids[33] = {};
    for (int i = 0; i < 33; ++i) {
        many[i] = nnh::component<K>(2);
        many[i].id = &ids[i];
    }
    CHECK(error_of({.max_entities = 4}, many, 32) == ErrorCode::None);
    CHECK(error_of({.max_entities = 4}, many, 33) == ErrorCode::InvalidArgument);
}

NNH_TEST(world_create_reports_out_of_memory) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    nnh::platform_fake::set_region_capacity(nnh::MemRegion::Heap, 2048);
    const ComponentDesc kinds[] = {nnh::component<Pos>(1000)};
    CHECK(error_of({.max_entities = 1000}, kinds, 1) == ErrorCode::OutOfMemory);
    nnh::platform_fake::reset();
}

NNH_TEST(world_takes_one_block_of_its_tag_sized_by_the_layout) {
    nnh::MemTag& tag = nnh::detail::entities_memory_tag();
    const nnh::MemStats before = nnh::mem::tag_stats(tag);
    const ComponentDesc kinds[] = {nnh::component<Pos>(8), nnh::component<Hp>(3)};
    {
        auto r = World::create({.max_entities = 8, .tags_per_entity = 2}, kinds);
        CHECK(r.ok());
        const nnh::MemStats now = nnh::mem::tag_stats(tag);
        CHECK(now.count == before.count + 1);
        CHECK(now.bytes - before.bytes ==
              nnh::detail::world_layout({.max_entities = 8, .tags_per_entity = 2}, kinds, 2).total);
    }
    CHECK(nnh::mem::tag_stats(tag).count == before.count);
    static NNH_MEM_TAG(level_world);
    {
        auto r = World::create({.max_entities = 8, .memory_tag = &NNH_MEM_TAG_REF(level_world)}, kinds);
        CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(level_world)).count == 1);
    }
    CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(level_world)).count == 0);
}

NNH_TEST(world_constructs_a_t_in_every_slot_and_destroys_them_all) {
    Counted::reset();
    {
        const ComponentDesc kinds[] = {nnh::component<Counted>(5)};
        auto r = World::create({.max_entities = 8}, kinds);
        CHECK(Counted::alive() == 6); // five slots and the scratch
        World& w = r.value();
        const nnh::Entity e = w.spawn();
        w.add<Counted>(e)->id = 4;
        w.add<Counted>(w.spawn(), Counted{});
        w.destroy(e);
        CHECK(Counted::alive() == 6);
    }
    CHECK(Counted::alive() == 0);
}

NNH_TEST(world_accepts_a_kind_that_cannot_be_copied_or_moved) {
    const ComponentDesc kinds[] = {nnh::component<Pinned>(2)};
    auto r = World::create({.max_entities = 2}, kinds);
    CHECK(r.ok());
    World& w = r.value();
    const nnh::Entity e = w.spawn();
    CHECK(w.add<Pinned>(e)->frame == 3);
}

NNH_TEST(world_moves_keep_entities_and_components) {
    World a = world_test::make_world();
    const nnh::Entity e = a.spawn();
    a.add<Pos>(e, {3, 4});
    World b = static_cast<World&&>(a);
    CHECK(b.alive(e) && b.get<Pos>(e)->y == 4);
    World c;
    c = static_cast<World&&>(b);
    CHECK(c.get<Pos>(e)->x == 3 && c.count() == 1);
}

NNH_TEST(world_stats_report_entities_and_kinds) {
    World w = world_test::make_world(4, 2);
    const nnh::Entity a = w.spawn();
    const nnh::Entity b = w.spawn();
    w.add<Pos>(a);
    w.add<Pos>(b);
    w.destroy(a);
    const nnh::WorldStats s = w.stats();
    CHECK(s.entities == 1 && s.peak == 2 && s.capacity == 4 && s.spawn_misses == 0 && s.add_misses == 0);
    const nnh::KindStats k = w.storage<Pos>().stats();
    CHECK(k.live == 1 && k.peak == 2 && k.capacity == 2 && k.misses == 0);
    CHECK(w.count() == 1 && w.capacity() == 4);
}

NNH_TEST(world_creation_and_use_allocate_only_the_block) {
    const auto news = alloc_counter::news();
    const int before = nnh::mem::live_allocations();
    {
        World w = world_test::make_world(64, 64);
        CHECK(nnh::mem::live_allocations() == before + 1);
        for (int i = 0; i < 1000; ++i) {
            const nnh::Entity e = w.spawn();
            w.add<Pos>(e);
            w.add<Vel>(e, {1, 1});
            w.add_tag(e, nnh::tag("thing"));
            w.each<Pos, Vel>([](nnh::Entity, Pos& p, Vel& v) { p.x += v.dx; });
            w.destroy(e);
            w.flush();
        }
        CHECK(nnh::mem::live_allocations() == before + 1);
    }
    CHECK(alloc_counter::news() == news);
    CHECK(nnh::mem::live_allocations() == before);
}

namespace {

World* g_world = nullptr;

void spawn_in_an_empty_world() { CHECK(!g_world->spawn()); }
void each_in_an_empty_world() {
    int n = 0;
    g_world->each<Pos>([&](nnh::Entity, Pos&) { ++n; });
    CHECK(n == 0);
}

} // namespace

NNH_TEST(world_moved_from_or_empty_asserts_on_commands_and_answers_nothing) {
    World a = world_test::make_world();
    const nnh::Entity e = a.spawn();
    World b = static_cast<World&&>(a);
    g_world = &a;
    CHECK(world_test::asserts_or_is_ignored(spawn_in_an_empty_world));
    CHECK(world_test::asserts_or_is_ignored(each_in_an_empty_world));
    CHECK(!a.alive(e) && a.get<Pos>(e) == nullptr && !a.destroy(e) && a.count() == 0 && a.capacity() == 0);
    World empty;
    CHECK(empty.stats().capacity == 0 && !empty.bound());
}

// Regression (spec 022 stage 9, clang ASan): T{} leaves a padded kind's padding bytes as the allocator gave them, so
// two worlds made alike differed byte for byte depending on what the heap held before (the prefab twin tests compare
// whole component arrays). The block now starts zeroed: worlds made alike are alike, whatever memory they got.
NNH_TEST(worlds_made_alike_have_the_same_component_bytes_whatever_memory_they_get) {
    struct Padded {
        bool on = false; // then 3 bytes of padding
        int count = 0;
    };
    const ComponentDesc kinds[] = {nnh::component<Padded>(4)};
    nnh::platform_fake::set_alloc_fill(0xAB);
    auto a = World::create({.max_entities = 4}, kinds);
    nnh::platform_fake::set_alloc_fill(0x5C);
    auto b = World::create({.max_entities = 4}, kinds);
    nnh::platform_fake::set_alloc_fill(-1);
    CHECK(a.ok() && b.ok());
    if (!a.ok() || !b.ok()) return;
    const nnh::detail::KindHot& x = nnh::WorldAccess::hot(a.value())->kinds[0];
    const nnh::detail::KindHot& y = nnh::WorldAccess::hot(b.value())->kinds[0];
    const auto* p = static_cast<const unsigned char*>(x.data);
    const auto* q = static_cast<const unsigned char*>(y.data);
    bool same = true;
    for (std::size_t i = 0; i < sizeof(Padded) * 5; ++i) same = same && p[i] == q[i]; // 4 slots and the scratch
    CHECK(same);
}
