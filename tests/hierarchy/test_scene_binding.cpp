// Scenes, flush and quarantine (spec 022 FR-12, T4.5): a world with nodes bound to a scene is flushed right before the
// scene's update and cleared (cascades included) right after its exit; node slots are quarantined until the flush; no
// new scene notification (the pass is the game's call).

#include "hierarchy_fixture.hpp"
#include "nnh/app.hpp"
#include "nnh/scene.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::World;
using namespace hierarchy_test;

namespace {

void frames(int n) {
    for (int i = 0; i < n; ++i) {
        CHECK(nnh::frame());
        nnh::scenes::update_and_draw();
    }
}

class Yard : public nnh::Scene {
public:
    Yard() {
        world = make_world(16, 16);
        world.bind(*this);
        tree = Hierarchy(world);
    }
    void on_enter() override {
        parent = spawn_node(world, {.position = {10, 0}});
        for (int i = 0; i < 3; ++i) tree.attach(spawn_node(world), parent);
    }
    void update() override {
        ++updates;
        if (updates == 1) {
            world.destroy(parent); // 4 node slots quarantined
            free_after_destroy = world.storage<Node>().count();
        }
        if (updates == 2) reused_slot = world.storage<Node>().slot_of(spawn_node(world)); // flushed before update 2
        computed = tree.propagate();
    }
    World world;
    Hierarchy tree;
    Entity parent{};
    int updates = 0, free_after_destroy = -1, reused_slot = -1, computed = -1;
};

class Other : public nnh::Scene {};

} // namespace

NNH_TEST(scene_bound_world_with_nodes_flushes_and_clears) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    {
        Yard yard;
        Other other;
        nnh::scenes::push(yard);
        frames(1);
        CHECK(yard.world.storage<Node>().count() == 0 && yard.updates == 1 && yard.free_after_destroy == 0);
        CHECK(yard.computed == 0);
        frames(1);
        CHECK(yard.reused_slot == 0 && yard.computed == 1); // the quarantined slots came back at the flush
        nnh::scenes::replace(other);
        frames(1);
        CHECK(yard.world.count() == 0); // cleared after the scene's exit
        nnh::scenes::pop();
        frames(1);
    }
    nnh::shutdown();
}
