// Placement (spec 021 T4.3, FR-7; map objects in T5.2): a spawn's position written into the prefab's Vec2 or Rect
// member; an override of the placement kind is constructed first and the position wins; a position for a prefab
// without a placement member asserts and the spawn goes on without it.

#include "prefab_fixture.hpp"

using nnh::Entity;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::Vec2;
using nnh::World;
using namespace prefab_test;

NNH_TEST(a_position_goes_into_a_vec2_member) {
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "mover", {.place = nnh::place(&Mover::v)}, Mover{{1, 2}, {3, 4}});
    World w = make_world();
    const Entity e = lib.spawn(w, p, {30, 40});
    CHECK(w.get<Mover>(e)->at.x == 1 && w.get<Mover>(e)->at.y == 2);
    CHECK(w.get<Mover>(e)->v.x == 30 && w.get<Mover>(e)->v.y == 40);
    CHECK(lib.value<Mover>(p)->v.x == 3); // the prefab is unchanged
}

NNH_TEST(a_position_from_code_sets_a_rect_members_x_and_y_only) {
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "body", {.place = nnh::place(&Body::box)}, Coin{1}, Body{{1, 2, 12, 14}, 3});
    World w = make_world();
    const Entity e = lib.spawn(w, p, {-5.5f, 7.25f});
    const nnh::Rect box = w.get<Body>(e)->box;
    CHECK(box.x == -5.5f && box.y == 7.25f && box.w == 12 && box.h == 14 && w.get<Body>(e)->layers == 3);
}

NNH_TEST(an_override_of_the_placement_kind_is_made_first_and_the_position_wins) {
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "body", {.place = nnh::place(&Body::box)}, Body{{1, 2, 12, 14}, 3});
    World w = make_world();
    const Entity e = lib.spawn_with(w, p, Vec2{8, 9}, Body{{100, 200, 4, 5}, 6});
    const Body* b = w.get<Body>(e);
    CHECK(b->box.x == 8 && b->box.y == 9 && b->box.w == 4 && b->box.h == 5 && b->layers == 6);
}

namespace {
PrefabLibrary* g_lib = nullptr;
World* g_w = nullptr;
Prefab g_p{};
Entity g_e{};
void spawn_with_a_position() { g_e = g_lib->spawn(*g_w, g_p, {5, 6}); }
} // namespace

NNH_TEST(a_position_without_a_placement_member_asserts_and_is_ignored) {
    PrefabLibrary lib = make_library();
    World w = make_world();
    g_lib = &lib;
    g_w = &w;
    g_p = must_add(lib, "plain", {}, Body{{1, 2, 3, 4}, 0});
    g_e = {};
    CHECK(asserts_or_is_ignored(spawn_with_a_position));
    if (!nnh::build::kAssertsEnabled) CHECK(w.get<Body>(g_e)->box.x == 1 && w.get<Body>(g_e)->box.y == 2);
    g_lib = nullptr;
    g_w = nullptr;
}

