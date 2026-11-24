// Following a node with the camera (spec 022 FR-15, T4.7): a CameraRig following a child's stored world after the pass
// gives the same camera as one following the hand-computed point; no camera change was needed.

#include "hierarchy_fixture.hpp"
#include "nnh/camera.hpp"

using nnh::CameraRig;
using nnh::Entity;
using nnh::Hierarchy;
using nnh::Reparent;
using nnh::Vec2;
using nnh::World;
using namespace hierarchy_test;

NNH_TEST(camera_follows_a_child_after_the_pass) {
    World w = make_world();
    Hierarchy h(w);
    const Entity cart = spawn_node(w, {.position = {100, 120}});
    const Entity rider = spawn_node(w, {.position = {0, -12}});
    h.attach(rider, cart, Reparent::KeepLocal);
    CameraRig a, b;
    for (int frame = 0; frame < 60; ++frame) {
        h.local(cart)->position.x += 2.5f;
        h.propagate();
        a.update(h.world_transform(rider)->position());
        b.update(Vec2{100 + 2.5f * static_cast<float>(frame + 1), 108});
        CHECK(a.position() == b.position());
    }
    CHECK(a.followed() == b.followed());
}
