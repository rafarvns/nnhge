// Fresh world transforms on demand (spec 022 FR-9, T4.2): world_now equals the pass's stored bits for every node of 50
// seeded hierarchies, before and after moves (then a pass); the identity without a node; the muzzle pattern.

#include "hierarchy_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Reparent;
using nnh::Vec2;
using nnh::World;
using namespace hierarchy_test;

namespace {

struct Rng {
    std::uint32_t s;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float f(int range) { return static_cast<float>(static_cast<int>(next() % (2u * range)) - range) / 4.0f; }
};

const float kScales[] = {1, -1, 0.5f, 2, -0.75f};

} // namespace

NNH_TEST(world_now_equals_the_pass_on_seeded_hierarchies) {
    for (std::uint32_t seed = 1; seed <= 50; ++seed) {
        World w = make_world(64, 64);
        Hierarchy h(w);
        Rng rng{seed * 0x85EBCA6Bu};
        Entity e[48];
        for (Entity& x : e) {
            x = spawn_node(w, {.position = {rng.f(400), rng.f(400)},
                               .rotation = (rng.next() & 1) != 0 ? 0.0f : static_cast<float>(rng.next() % 4096) / 512.0f,
                               .scale = {kScales[rng.next() % 5], kScales[rng.next() % 5]}});
        }
        for (int i = 1; i < 48; ++i) {
            const Entity parent = e[rng.next() % static_cast<std::uint32_t>(i)];
            if (h.can_attach(e[i], parent, Reparent::KeepLocal) == nnh::AttachCheck::Ok)
                h.attach(e[i], parent, Reparent::KeepLocal);
        }
        h.propagate();
        for (Entity x : e) CHECK(same_bits(h.world_now(x), *h.world_transform(x)));
        // Moves: world_now sees them at once; the pass stores the same bits.
        for (int i = 0; i < 48; i += 3) {
            h.local(e[i])->rotation += 0.25f;
            h.local(e[i])->position.x += 1.5f;
        }
        nnh::WorldTransform2D fresh[48];
        for (int i = 0; i < 48; ++i) fresh[i] = h.world_now(e[i]);
        h.propagate();
        for (int i = 0; i < 48; ++i) CHECK(same_bits(fresh[i], *h.world_transform(e[i])));
    }
}

NNH_TEST(world_now_without_a_node_is_the_identity) {
    World w = make_world();
    Hierarchy h(w);
    CHECK(same_bits(h.world_now(w.spawn()), nnh::WorldTransform2D{}));
    CHECK(same_bits(h.world_now(Entity{}), nnh::WorldTransform2D{}));
}

NNH_TEST(world_now_gives_the_muzzle_in_the_frame_the_gun_turned) {
    World w = make_world();
    Hierarchy h(w);
    const Entity ship = spawn_node(w, {.position = {100, 100}});
    const Entity gun = spawn_node(w, {.position = {10, 0}});
    const Entity muzzle = spawn_node(w, {.position = {6, 0}});
    h.attach(gun, ship, Reparent::KeepLocal);
    h.attach(muzzle, gun, Reparent::KeepLocal);
    h.propagate();
    h.local(gun)->rotation = nnh::kQuarterTurn; // aimed down, this frame
    const Vec2 from = nnh::transform_point(h.world_now(muzzle).map, {0, 0});
    CHECK(from == (Vec2{110, 106}));
    CHECK(h.world_transform(muzzle)->position() == (Vec2{116, 100})); // the stored one: last pass
}
