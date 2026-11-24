// Keep-world and keep-local reparenting (spec 022 FR-10, AC-3, T3.3): keep-world leaves the stored world unchanged and
// the local's world after a pass lands within the bound (exactly without shear for power-of-two scales and quarter
// turns); keep-local computes the stored world; descendants keep their locals; attach then detach round trips.

#include "hierarchy_fixture.hpp"

#include <cmath>

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Reparent;
using nnh::Transform2D;
using nnh::Vec2;
using nnh::World;
using nnh::WorldTransform2D;
using namespace hierarchy_test;

namespace {

bool near(Vec2 a, Vec2 b, float eps) { return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps; }

} // namespace

NNH_TEST(keep_world_leaves_the_stored_world_and_lands_there_after_a_pass) {
    World w = make_world();
    Hierarchy h(w);
    const Entity hand = spawn_node(w, {.position = {50, 40}, .rotation = 0.7f, .scale = {1.5f, 1.5f}});
    const Entity item = spawn_node(w, {.position = {132.5f, 40.25f}, .rotation = -0.2f});
    const WorldTransform2D before = *h.world_transform(item);
    CHECK(h.attach(item, hand));
    CHECK(same_bits(*h.world_transform(item), before));
    h.propagate();
    CHECK(near(h.world_transform(item)->position(), before.position(), 1.0f / 32));
    CHECK(std::fabs(h.world_transform(item)->rotation - before.rotation) < 1e-6f);
    // Dropped (detached keeping the world): it stays where it was drawn.
    const WorldTransform2D held = *h.world_transform(item);
    CHECK(h.detach(item));
    CHECK(same_bits(*h.world_transform(item), held));
    h.propagate();
    CHECK(near(h.world_transform(item)->position(), held.position(), 1.0f / 32));
}

NNH_TEST(keep_world_is_exact_for_quarter_turns_and_powers_of_two) {
    World w = make_world();
    Hierarchy h(w);
    const Entity parent = spawn_node(w, {.position = {64, -32}, .rotation = nnh::kQuarterTurn, .scale = {2, 2}});
    const Entity item = spawn_node(w, {.position = {100, 20}, .rotation = nnh::kHalfTurn, .scale = {0.5f, 0.5f}});
    const WorldTransform2D before = *h.world_transform(item);
    CHECK(h.attach(item, parent));
    CHECK(h.local(item)->scale == (Vec2{0.25f, 0.25f}));
    h.propagate();
    const WorldTransform2D after = *h.world_transform(item);
    CHECK(after.position() == before.position() && after.rotation == before.rotation && after.scale == before.scale);
    CHECK(h.detach(item));
    h.propagate();
    CHECK(same_bits(*h.world_transform(item), before)); // the round trip returns the original
}

NNH_TEST(keep_local_computes_the_world_under_the_new_parent) {
    World w = make_world();
    Hierarchy h(w);
    const Entity ship = spawn_node(w, {.position = {100, 60}, .rotation = nnh::kQuarterTurn});
    const Entity gun = spawn_node(w, {.position = {12, -4}});
    CHECK(h.attach(gun, ship, Reparent::KeepLocal));
    CHECK(h.local(gun)->position == (Vec2{12, -4}));
    CHECK(h.world_transform(gun)->position() == (Vec2{104, 72}));
    CHECK(same_bits(*h.world_transform(gun), nnh::compose(*h.world_transform(ship), {.position = {12, -4}})));
    CHECK(h.detach(gun, Reparent::KeepLocal));
    CHECK(same_bits(*h.world_transform(gun), nnh::to_world({.position = {12, -4}})));
}

NNH_TEST(reparenting_keeps_the_descendants_locals) {
    World w = make_world();
    Hierarchy h(w);
    const Entity a = spawn_node(w, {.position = {10, 0}});
    const Entity b = spawn_node(w, {.position = {0, 10}, .rotation = 1});
    const Entity c = spawn_node(w, {.position = {5, 5}});
    const Entity d = spawn_node(w, {.position = {1, 1}, .rotation = 0.5f});
    h.attach(c, b, Reparent::KeepLocal);
    h.attach(d, c, Reparent::KeepLocal);
    const Transform2D dl = *h.local(d), cl_before = *h.local(c);
    CHECK(h.attach(b, a));
    CHECK(h.local(d)->position == dl.position && h.local(d)->rotation == dl.rotation);
    CHECK(h.local(c)->position == cl_before.position && h.depth(d) == 3);
    h.propagate();
    CHECK(same_bits(*h.world_transform(d), h.world_now(d)));
}

NNH_TEST(keep_world_round_trips_over_seeded_parents) {
    World w = make_world();
    Hierarchy h(w);
    for (int i = 0; i < 100; ++i) {
        const float f = static_cast<float>(i);
        const Entity p = spawn_node(w, {.position = {f * 13 - 600, 200 - f * 7}, .rotation = f * 0.37f,
                                        .scale = {0.5f + f / 64, 0.5f + f / 64}});
        const Entity item = spawn_node(w, {.position = {f * 3, -f * 5}, .rotation = -f * 0.13f, .scale = {1.25f, 1.25f}});
        const WorldTransform2D before = *h.world_transform(item);
        CHECK(h.attach(item, p));
        h.propagate();
        CHECK(near(h.world_transform(item)->position(), before.position(), 1.0f / 32));
        CHECK(h.detach(item));
        h.propagate();
        CHECK(near(h.world_transform(item)->position(), before.position(), 1.0f / 32));
        CHECK(std::fabs(h.world_transform(item)->scale.x - before.scale.x) < 1e-5f);
        w.destroy(p);
        w.destroy(item);
        w.flush();
    }
}
