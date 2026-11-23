// Misuse and cost (spec 020 FR-22, FR-23; T3.5): FR-23's list asserts in Debug/Development and is harmless in
// Release; nothing allocates and nothing logs in the normal path; a world destroyed from its own hook is freed by the
// outermost operation.

#include "alloc_counter.hpp"
#include "entities/world_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/memory.hpp"
#include "world_fixture.hpp"

#include <cstring>

using nnh::ComponentDesc;
using nnh::Entity;
using nnh::World;
using world_test::Hp;
using world_test::Pos;
using world_test::Vel;

namespace {

World* g_w = nullptr;
Entity g_e{};

void find_with_a_null_buffer() { CHECK(g_w->find(nnh::tag("x"), nullptr, 4) == 0); }
void find_with_a_negative_capacity() {
    Entity out[2];
    CHECK(g_w->find(nnh::tag("x"), out, -1) == 0);
}
void add_tag_none() { CHECK(!g_w->add_tag(g_e, nnh::Tag::None)); }
void bind_twice_without_a_scene_stack() {
    nnh::Scene* none = nullptr;
    (void)none;
    g_w->each<Pos>([](Entity, Pos&) {
        struct S : nnh::Scene {};
        static S s;
        g_w->bind(s); // inside an iteration
    });
}

} // namespace

NNH_TEST(world_misuse_list_asserts_or_is_harmless) {
    World w = world_test::make_world();
    g_w = &w;
    g_e = w.spawn();
    w.add<Pos>(g_e);
    CHECK(world_test::asserts_or_is_ignored(find_with_a_null_buffer));
    CHECK(world_test::asserts_or_is_ignored(find_with_a_negative_capacity));
    CHECK(world_test::asserts_or_is_ignored(add_tag_none));
    CHECK(world_test::asserts_or_is_ignored(bind_twice_without_a_scene_stack));
    world_test::recover(w);
    CHECK(!w.bound() && w.alive(g_e));
}

NNH_TEST(world_normal_use_allocates_nothing_and_logs_nothing) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    World w = world_test::make_world(256, 256);
    const auto news = alloc_counter::news();
    const int engine = nnh::mem::live_allocations();
    const char* const line_before = nnh::platform_fake::state().last_console_line;
    char before[256];
    std::strncpy(before, line_before, sizeof before - 1);
    before[sizeof before - 1] = 0;
    std::uint32_t seed = 7;
    for (int i = 0; i < 100000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const int op = static_cast<int>(seed >> 28);
        const Entity e = w.storage<Pos>().owner(static_cast<int>((seed >> 8) % 256));
        if (op < 4 && w.count() < 200) {
            const Entity n = w.spawn();
            w.add<Pos>(n);
            if (op & 1) w.add<Vel>(n);
            w.add_tag(n, nnh::tag("a"));
        } else if (op < 8) {
            w.destroy(e);
        } else if (op < 10) {
            w.each<Pos, Vel>([](Entity, Pos& p, Vel& v) { p.x += v.dx; });
        } else if (op < 12) {
            Entity out[4];
            (void)w.find(nnh::tag("a"), out, 4);
        } else if (op < 14) {
            w.remove<Vel>(e);
        } else {
            w.flush();
        }
    }
    CHECK(alloc_counter::news() == news);
    CHECK(nnh::mem::live_allocations() == engine);
    CHECK(std::strcmp(before, nnh::platform_fake::state().last_console_line) == 0);
    nnh::shutdown();
}

namespace {

World* g_doomed = nullptr;

void destroy_the_world_from_its_own_hook() {
    const ComponentDesc kinds[] = {nnh::component<Hp>(4, [](World&, Entity, Hp&) {
        *g_doomed = World{}; // the block is freed by the outermost destroy()
    })};
    auto r = World::create({.max_entities = 4}, kinds);
    *g_doomed = static_cast<World&&>(r.value());
    const Entity a = g_doomed->spawn();
    g_doomed->add<Hp>(a);
    g_doomed->add<Hp>(g_doomed->spawn());
    g_doomed->destroy(a);
}

} // namespace

NNH_TEST(destroying_a_world_from_its_own_hook_asserts_or_is_freed_by_the_outermost_call) {
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::detail::entities_memory_tag());
    World holder;
    g_doomed = &holder;
    const bool crashed = world_test::crashes(destroy_the_world_from_its_own_hook);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) {
        // Debug: the assert stopped inside the hook, before anything was freed. The block is still `holder`'s.
        world_test::recover(holder);
        holder = World{};
    }
    CHECK(!holder.alive(Entity{}) && holder.count() == 0);
    CHECK(nnh::mem::tag_stats(nnh::detail::entities_memory_tag()).count == before.count);
}
