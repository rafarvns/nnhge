// Per-world module words and the dying/busy queries (spec 022 T2.1; entities-internal, for the hierarchy): zero at
// creation and kept by clear(); dying inside a destroy's hooks and while queued, false after; busy inside each() and
// remove hooks.

#include "entities/entities_internal.hpp"
#include "world_fixture.hpp"

using nnh::Entity;
using nnh::World;
using world_test::Hp;
using world_test::Pos;

namespace {

struct Probe {
    static inline World* world = nullptr;
    static inline Entity other{};
    static inline bool dying_self = false, dying_other = false, busy = false;
};

World make_hooked() {
    const nnh::ComponentDesc kinds[] = {
        nnh::component<Pos>(8, [](World& w, Entity e, Pos&) {
            Probe::dying_self = nnh::detail::world_dying(w, e);
            Probe::busy = nnh::detail::world_busy(w);
            if (Probe::other) {
                w.destroy(Probe::other); // queued: hooks are running
                Probe::dying_other = nnh::detail::world_dying(w, Probe::other);
            }
        }),
        nnh::component<Hp>(8)};
    auto r = World::create({.max_entities = 8}, kinds);
    CHECK(r.ok());
    return static_cast<World&&>(r.value());
}

} // namespace

NNH_TEST(world_module_state_is_zero_at_creation_and_kept_by_clear) {
    World w = world_test::make_world();
    nnh::detail::WorldModuleState& m = nnh::detail::world_module_state(w);
    CHECK(m.walks == nullptr);
    for (std::uint32_t word : m.words) CHECK(word == 0);
    m.words[2] = 42;
    w.clear();
    CHECK(nnh::detail::world_module_state(w).words[2] == 42);
    World empty;
    CHECK(nnh::detail::world_module_state(empty).words[2] == 0);
}

NNH_TEST(world_dying_inside_hooks_and_while_queued_then_false) {
    World w = make_hooked();
    const Entity a = w.spawn();
    const Entity b = w.spawn();
    w.add<Pos>(a);
    w.add<Hp>(b);
    CHECK(!nnh::detail::world_dying(w, a) && !nnh::detail::world_busy(w));
    Probe::other = b;
    CHECK(w.destroy(a));
    CHECK(Probe::dying_self && Probe::dying_other && Probe::busy);
    CHECK(!w.alive(a) && !w.alive(b));
    CHECK(!nnh::detail::world_dying(w, a) && !nnh::detail::world_dying(w, b)); // stale: not dying
    Probe::other = {};
    World empty;
    CHECK(!nnh::detail::world_dying(empty, a) && !nnh::detail::world_busy(empty));
}

NNH_TEST(world_busy_inside_each) {
    World w = world_test::make_world();
    const Entity e = w.spawn();
    w.add<Pos>(e);
    bool busy = false;
    w.each<Pos>([&](Entity, Pos&) { busy = nnh::detail::world_busy(w); });
    CHECK(busy && !nnh::detail::world_busy(w));
}
