// Misuse and cost (spec 021 T4.6, FR-14, FR-15): FR-15's list asserts in Debug/Development and is harmless in
// Release; after creation nothing allocates and nothing logs in the normal path; a library destroyed from its own spawn
// hook is freed when the outermost spawn returns.

#include "alloc_counter.hpp"
#include "nnh/app.hpp"
#include "nnh/memory.hpp"
#include "prefab_fixture.hpp"

#include <cstring>
#include <memory>

using nnh::Entity;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::World;
using namespace prefab_test;

namespace {

PrefabLibrary* g_lib = nullptr;
World* g_w = nullptr;
Prefab g_p{};
Entity g_e{};

void spawn_null() { g_e = g_lib->spawn(*g_w, Prefab{}); }
void spawn_foreign() { g_e = g_lib->spawn(*g_w, g_p); }
void spawn_from_an_empty_library() {
    PrefabLibrary empty;
    g_e = empty.spawn(*g_w, g_p);
}
void spawn_into_an_empty_world() {
    World empty;
    g_e = g_lib->spawn(empty, g_p);
}
void spawn_more_tags_than_the_world_holds() { g_e = g_lib->spawn(*g_w, g_p); }

} // namespace

NNH_TEST(prefab_misuse_list_asserts_or_is_harmless) {
    PrefabLibrary lib = make_library();
    PrefabLibrary other = make_library();
    World w = make_world(8, 8, 2);
    g_lib = &lib;
    g_w = &w;
    const Prefab mine = must_add(lib, "coin", {}, Coin{1});
    g_p = must_add(other, "foreign", {}, Coin{1});
    CHECK(asserts_or_is_ignored(spawn_null) && !g_e);
    CHECK(asserts_or_is_ignored(spawn_foreign) && !g_e);
    g_p = mine;
    CHECK(asserts_or_is_ignored(spawn_from_an_empty_library) && !g_e);
    CHECK(asserts_or_is_ignored(spawn_into_an_empty_world) && !g_e);
    CHECK(w.count() == 0 && lib.stats().spawns == 0 && lib.stats().misses == 0); // misuse is not a miss
    // Three tags into a world of two per entity: Release keeps the first two.
    g_p = must_add(lib, "tagged", {.tags = {kEnemy, kFx, kFlying}}, Coin{2});
    CHECK(asserts_or_is_ignored(spawn_more_tags_than_the_world_holds));
    if (!nnh::build::kAssertsEnabled) CHECK(w.has_tag(g_e, kEnemy) && w.has_tag(g_e, kFx) && !w.has_tag(g_e, kFlying));
    world_test::recover(w);
    g_lib = nullptr;
    g_w = nullptr;
}

NNH_TEST(prefab_normal_use_allocates_nothing_and_logs_nothing) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    {
        PrefabLibrary lib = make_library();
        const Prefab walker = must_add(lib, "walker", {.tags = {kEnemy}, .place = nnh::place(&Body::box)}, Body{},
                                       Coin{1}, Big{});
        const Prefab flyer = must_add(lib, "flyer", {.base = walker, .tags = {kFlying}}, Bob{});
        World w = make_world(256, 256);
        const auto news = alloc_counter::news();
        const int engine = nnh::mem::live_allocations();
        char before[256];
        std::strncpy(before, nnh::platform_fake::state().last_console_line, sizeof before - 1);
        before[sizeof before - 1] = 0;
        std::uint32_t seed = 11;
        Entity last{};
        for (int i = 0; i < 100000; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const int op = static_cast<int>(seed >> 28);
            if (op < 6 && w.count() < 200) {
                last = (op & 1) ? lib.spawn(w, walker, {1, 2}) : lib.spawn_with(w, flyer, Coin{3});
            } else if (op < 12) {
                w.destroy(w.storage<Coin>().owner(static_cast<int>((seed >> 8) % 256)));
            } else {
                w.flush();
            }
        }
        CHECK(alloc_counter::news() == news);
        CHECK(nnh::mem::live_allocations() == engine);
        CHECK(std::strcmp(before, nnh::platform_fake::state().last_console_line) == 0);
        CHECK(lib.stats().spawns > 10000 && lib.stats().misses == 0);
    }
    nnh::shutdown();
}

namespace {

PrefabLibrary* g_doomed = nullptr;

void destroy_the_library_from_its_own_hook() {
    *g_doomed = make_library();
    const Prefab inner = must_add(*g_doomed, "inner", {.on_spawn = [](World&, Entity, const nnh::SpawnInfo&) {
                                                         *g_doomed = PrefabLibrary{}; // freed after the outer spawn
                                                     }},
                                  Coin{1});
    struct Outer {
        Prefab inner;
    };
    static Outer outer;
    outer.inner = inner;
    const Prefab p = must_add(*g_doomed, "outer", {.on_spawn = [](World& w, Entity, const nnh::SpawnInfo&) {
                                                     g_doomed->spawn(w, outer.inner);
                                                 }},
                              Coin{2}, Counted{});
    g_e = g_doomed->spawn(*g_w, p);
}

} // namespace

namespace {

std::unique_ptr<PrefabLibrary> g_owned;
PrefabLibrary* g_raw = nullptr;

void delete_the_library_from_its_own_hook() {
    g_owned.reset(new PrefabLibrary(make_library()));
    g_raw = g_owned.get();
    const Prefab p = must_add(*g_owned, "p", {.on_spawn = [](World&, Entity, const nnh::SpawnInfo&) {
                                                g_owned.reset(); // the destructor itself, inside the hook
                                            }},
                              Coin{1});
    g_e = g_owned->spawn(*g_w, p);
}

} // namespace

NNH_TEST(deleting_a_library_from_its_own_hook_asserts_or_is_freed_by_the_spawn) {
    // Mutation testing (plan 021 T8.1): the destructor's own check, not only move-assignment's.
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::detail::prefabs_memory_tag());
    World w = make_world();
    g_w = &w;
    const bool crashed = crashes(delete_the_library_from_its_own_hook);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) {
        // Debug: stopped inside the destructor before it changed anything (unique_ptr had already let go of the
        // object): take the block out of the half-destroyed object, free it, and free the object's storage.
        nnh::detail::PrefabBlock* b = nnh::PrefabAccess::block(*g_raw);
        CHECK(b != nullptr);
        if (b != nullptr) b->spawn_depth = 0;
        { PrefabLibrary rescued(static_cast<PrefabLibrary&&>(*g_raw)); }
        ::operator delete(static_cast<void*>(g_raw));
        world_test::recover(w);
    } else {
        CHECK(w.alive(g_e) && !g_owned);
    }
    CHECK(nnh::mem::tag_stats(nnh::detail::prefabs_memory_tag()).count == before.count);
    g_raw = nullptr;
    g_w = nullptr;
}

NNH_TEST(destroying_a_library_from_its_own_hook_asserts_or_is_freed_by_the_outermost_spawn) {
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::detail::prefabs_memory_tag());
    Counted::reset();
    PrefabLibrary holder;
    World w = make_world();
    g_doomed = &holder;
    g_w = &w;
    const bool crashed = crashes(destroy_the_library_from_its_own_hook);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) {
        // Debug: the assert stopped inside the inner hook, before anything was freed. The block is still `holder`'s.
        nnh::detail::PrefabBlock* b = nnh::PrefabAccess::block(holder);
        if (b != nullptr) b->spawn_depth = 0;
        holder = PrefabLibrary{};
        world_test::recover(w);
    } else {
        CHECK(w.alive(g_e) && w.count() == 2); // both copies made; the block freed after them
    }
    CHECK(holder.count() == 0);
    CHECK(nnh::mem::tag_stats(nnh::detail::prefabs_memory_tag()).count == before.count);
    w = World{};
    CHECK(Counted::alive() == 0);
    g_doomed = nullptr;
    g_w = nullptr;
}
