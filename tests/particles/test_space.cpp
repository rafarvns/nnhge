// World and local space (spec 024 FR-14; T3.3): a world trail stays behind; local particles follow a moving, rotated
// and mirrored transform; gravity local or world; emit_at in either space.

#include <cmath>

#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::Transform2D;
using nnh::Vec2;

NNH_TEST(world_particles_stay_behind_a_moving_emitter) {
    ParticleEmitter e = particles_test::make({}, 8);
    e.emit(1);
    e.set_position({50, 0});
    e.update();
    CHECK(e.particle(0).position == (Vec2{0, 0}));
    CHECK(e.bounds().x == 0);
}

NNH_TEST(local_particles_follow_the_emitter) {
    ParticleDesc d;
    d.space = nnh::ParticleSpace::Local;
    ParticleEmitter e = particles_test::make(d, 8);
    e.emit_at(Vec2{10, 0}, 1);
    e.set_transform(Transform2D{.position = {100, 50}, .rotation = nnh::kQuarterTurn, .scale = {-1, 1}});
    e.update();
    CHECK(e.particle(0).position == (Vec2{10, 0})); // stored in the emitter's frame
    const nnh::Rect r = e.bounds();                // world: the point turned a quarter and mirrored: (100, 40)
    CHECK(std::fabs(r.x - 100) < 1e-4f && std::fabs(r.y - 40) < 1e-4f);
}

NNH_TEST(gravity_turns_with_the_emitter_unless_it_is_world_gravity) {
    ParticleDesc d;
    d.gravity = {0, 3600}; // 1 px per frame² "down" in the emitter's frame
    d.lifetime = {nnh::Frames{50}, nnh::Frames{50}};
    ParticleEmitter turned = particles_test::make(d, 4);
    turned.set_transform(Transform2D{.rotation = nnh::kQuarterTurn}, true);
    turned.emit(1);
    turned.update();
    CHECK(turned.particle(0).velocity.x == -1 && std::fabs(turned.particle(0).velocity.y) < 1e-6f);
    d.world_gravity = true;
    ParticleEmitter world = particles_test::make(d, 4);
    world.set_transform(Transform2D{.rotation = nnh::kQuarterTurn}, true);
    world.emit(1);
    world.update();
    CHECK(world.particle(0).velocity.x == 0 && world.particle(0).velocity.y == 1);
}

NNH_TEST(local_space_world_gravity_goes_through_the_inverse) {
    ParticleDesc d;
    d.space = nnh::ParticleSpace::Local;
    d.gravity = {0, 3600};
    d.world_gravity = true;
    d.lifetime = {nnh::Frames{50}, nnh::Frames{50}};
    ParticleEmitter e = particles_test::make(d, 4);
    e.set_transform(Transform2D{.rotation = nnh::kQuarterTurn});
    e.emit(1);
    e.update();
    // A frame turned a quarter clockwise has its +x along world down.
    CHECK(std::fabs(e.particle(0).velocity.x - 1) < 1e-6f && std::fabs(e.particle(0).velocity.y) < 1e-6f);
}

NNH_TEST(emit_at_a_transform_uses_its_orientation) {
    ParticleDesc d;
    d.speed = {60, 60};
    ParticleEmitter e = particles_test::make(d, 4);
    nnh::WorldTransform2D at = nnh::to_world(Transform2D{.position = {7, 8}, .rotation = nnh::kHalfTurn});
    e.emit_at(at, 1);
    CHECK(e.particle(0).position == (Vec2{7, 8}) && e.particle(0).velocity.x == -1);
    CHECK(e.transform().position() == (Vec2{0, 0})); // the emitter didn't move
}

NNH_TEST(mirrored_world_emitter_turns_rotations_the_other_way) {
    ParticleDesc d;
    d.rotation = {0.5f, 0.5f};
    d.spin = {6, 6};
    ParticleEmitter e = particles_test::make(d, 4);
    e.set_transform(Transform2D{.rotation = 1, .scale = {-1, 1}});
    e.emit(1);
    CHECK(e.particle(0).rotation == 0.5f); // 1 − 0.5, as gfx::transformed
    e.update();
    CHECK(std::fabs(e.particle(0).rotation - (0.5f - 0.1f)) < 1e-6f);
}
