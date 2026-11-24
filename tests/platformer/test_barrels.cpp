// The platformer's turret barrels (spec 022 AC-4, T6.2): each turret spawned by the level's prefab has a barrel child
// at its centre; the barrel turns to one of 8 angles (the 8 directions of aim()) while the turret sees the player; a
// shot starts at the barrel's muzzle point (exact on the axes) and moves with aim()'s velocity.

#include "../../examples/platformer/level_prefabs.hpp"
#include "../../examples/platformer/turrets.hpp"
#include "nnh/hierarchy.hpp"
#include "test.hpp"

#include <cmath>

using nnh::Rect;
using nnh::Vec2;

namespace {

struct Yard {
    nnh::World world = static_cast<nnh::World&&>(level::create_world().value());
    level::LevelPrefabs prefabs = static_cast<level::LevelPrefabs&&>(level::create_level_prefabs(nullptr).value());
    nnh::Hierarchy tree{world};
};

} // namespace

NNH_TEST(barrels_turn_to_eight_angles) {
    const Vec2 v[8] = {{2, 0}, {1.375f, 1.375f}, {0, 2}, {-1.375f, 1.375f}, {-2, 0}, {-1.375f, -1.375f}, {0, -2},
                       {1.375f, -1.375f}};
    for (int k = 0; k < 8; ++k) {
        const float want = static_cast<float>(k <= 4 ? k : k - 8) * nnh::kQuarterTurn * 0.5f;
        CHECK(TurretBook::barrel_angle(v[k]) == want);
    }
}

NNH_TEST(barrels_are_children_at_the_turret_centre) {
    Yard y;
    const nnh::Entity t = y.prefabs.library.spawn_with(y.world, y.prefabs.turret, level::Body{{32, 48, 16, 16}, layers::kEnemy});
    const nnh::Entity barrel = y.tree.first_child(t);
    CHECK(barrel && y.world.has<level::Barrel>(barrel) && y.world.has_tag(barrel, level::kBarrelTag));
    CHECK(y.tree.local(t)->position == (Vec2{40, 56}) && y.tree.local(barrel)->position == Vec2{});
    y.tree.propagate();
    CHECK(y.tree.world_transform(barrel)->position() == (Vec2{40, 56}));
}

NNH_TEST(barrels_muzzle_points_are_exact_on_the_axes) {
    Yard y;
    const nnh::Entity t = y.prefabs.library.spawn_with(y.world, y.prefabs.turret, level::Body{{32, 48, 16, 16}, layers::kEnemy});
    const nnh::Entity barrel = y.tree.first_child(t);
    const Vec2 axis[4] = {{2, 0}, {0, 2}, {-2, 0}, {0, -2}};
    const Vec2 want[4] = {{50, 56}, {40, 66}, {30, 56}, {40, 46}};
    for (int i = 0; i < 4; ++i) {
        y.tree.local(barrel)->rotation = TurretBook::barrel_angle(axis[i]);
        CHECK(TurretBook::muzzle(y.tree, barrel) == want[i]);
    }
    y.tree.local(barrel)->rotation = TurretBook::barrel_angle({1.375f, 1.375f});
    const Vec2 d = TurretBook::muzzle(y.tree, barrel);
    CHECK(std::fabs(d.x - 47.0710678f) < 1e-4f && std::fabs(d.y - 63.0710678f) < 1e-4f);
}
