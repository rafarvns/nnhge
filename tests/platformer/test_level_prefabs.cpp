// The platformer's prefabs (examples/platformer/level_prefabs.hpp, spec 021 T6.1, AC-4): the three prefabs' kinds,
// tags, values and placements, and each spawn equal to what the hand-written code made before (a coin and a turret
// from map objects, a sparkle with its fade), on twin worlds.

#include "../prefabs/twin.hpp"
#include "nnh/tilemap.hpp"
#include "nnh/tween.hpp"
#include "platformer/level_prefabs.hpp"
#include "test.hpp"

#include <cstring>
#include <utility>

using level::Body;
using level::Coin;
using level::Spark;
using level::Turret;
using level::Velocity;
using nnh::Entity;
using nnh::World;

namespace {

World make_world(nnh::TweenSet* tweens = nullptr) {
    return static_cast<World&&>(level::create_world(tweens).value());
}

level::LevelPrefabs make_prefabs(nnh::TweenSet* tweens = nullptr) {
    return static_cast<level::LevelPrefabs&&>(level::create_level_prefabs(tweens).value());
}

/// A map with a coin (value 5), a coin without `value`, a turret, a door and a spawn point.
nnh::Tilemap things() {
    nnh::Tilemap map = static_cast<nnh::Tilemap&&>(nnh::Tilemap::create({.width = 16, .height = 8}).value());
    const int layer = map.add_object_layer("things").value();
    const int c = map.add_object(layer, {.name = "coin", .type = "coin", .bounds = {34, 18, 12, 12}}).value();
    CHECK(map.set_int(nnh::PropertyOwner::object(c), "value", 5).ok());
    CHECK(map.add_object(layer, {.name = "coin", .type = "coin", .bounds = {50, 18, 12, 12}}).ok());
    CHECK(map.add_object(layer, {.name = "t", .type = "turret", .bounds = {64, 96, 16, 16}}).ok());
    CHECK(map.add_object(layer, {.name = "door", .type = "door", .bounds = {0, 0, 16, 32}}).ok());
    CHECK(map.add_object(layer, {.name = "spawn", .shape = nnh::ObjectShape::Point, .bounds = {8, 8}}).ok());
    return map;
}

} // namespace

NNH_TEST(the_level_prefabs_hold_their_kinds_tags_and_placements) {
    level::LevelPrefabs p = make_prefabs();
    nnh::KindId kinds[2] = {};
    nnh::Tag tags[2] = {};
    CHECK(p.library.kinds(p.coin, kinds, 2) == 2 && kinds[0] == nnh::kind_id<Body>() && kinds[1] == nnh::kind_id<Coin>());
    CHECK(p.library.tags(p.coin, tags, 2) == 1 && tags[0] == level::kCoinTag);
    CHECK(p.library.value<Body>(p.coin)->layers == layers::kCoin && p.library.value<Coin>(p.coin)->value == 1);
    nnh::KindId turret_kinds[4] = {};
    CHECK(p.library.kinds(p.turret, turret_kinds, 4) == 3 && turret_kinds[1] == nnh::kind_id<Turret>() &&
          turret_kinds[2] == nnh::kind_id<nnh::Node>());
    nnh::Prefab children[2] = {};
    CHECK(p.library.children(p.turret, children, 2) == 1 && children[0] == p.barrel); // spec 022: its barrel
    CHECK(p.library.kinds(p.barrel, kinds, 2) == 2 && kinds[1] == nnh::kind_id<level::Barrel>());
    CHECK(p.library.value<Body>(p.turret)->layers == layers::kEnemy);
    CHECK(p.library.kinds(p.spark, kinds, 2) == 2 && kinds[0] == nnh::kind_id<Spark>() &&
          kinds[1] == nnh::kind_id<Velocity>());
    CHECK(p.library.tags(p.spark, tags, 2) == 1 && tags[0] == level::kFxTag);
    CHECK(p.library.value<Spark>(p.spark)->alpha == 1.0f);
    CHECK(p.library.stats().prefabs == 4 && p.library.stats().entries == 9);
}

NNH_TEST(a_map_turret_has_a_barrel_child_at_its_centre) {
    // Spec 022 T6.1: the turret's Body keeps the map's rectangle (the bytes of the pre-barrel turret); its node sits at
    // the box's centre and its first child is the barrel.
    level::LevelPrefabs p = make_prefabs();
    World w = make_world();
    auto made = nnh::Tilemap::create({.width = 8, .height = 4, .max_object_layers = 1, .max_objects = 4,
                                      .max_properties = 4, .string_bytes = 256});
    nnh::Tilemap map = std::move(made.value());
    const int layer = map.add_object_layer("things").value();
    CHECK(map.add_object(layer, {.name = "t", .type = "turret", .bounds = {32, 48, 16, 16}}).ok());
    CHECK(p.library.spawn_map_objects(w, map).spawned == 1);
    nnh::Hierarchy tree(w);
    const nnh::Entity turret = w.first(level::kTurretTag);
    const Body expected{{32, 48, 16, 16}, layers::kEnemy};
    CHECK(std::memcmp(w.get<Body>(turret), &expected, sizeof expected) == 0);
    CHECK(tree.local(turret)->position == (nnh::Vec2{40, 56}) && tree.child_count(turret) == 1);
    const nnh::Entity barrel = tree.first_child(turret);
    CHECK(w.has<level::Barrel>(barrel) && w.has_tag(barrel, level::kBarrelTag) && barrel.slot() == turret.slot() + 1);
}

NNH_TEST(map_coins_equal_the_hand_written_coins) {
    level::LevelPrefabs p = make_prefabs();
    nnh::Tilemap map = things();
    World a = make_world();
    World b = make_world();
    const nnh::MapSpawnSummary s = p.library.spawn_map_objects(a, map);
    CHECK(s.spawned == 3 && s.skipped == 2 && s.unknown == 0 && s.misses == 0);
    // Before prefabs: spawn_coin(world, bounds, value) and TurretBook::spawn_turret(world, box), in object order (spec
    // 022: the turret with its node and barrel).
    for (const auto& [bounds, value] : {std::pair{nnh::Rect{34, 18, 12, 12}, 5}, std::pair{nnh::Rect{50, 18, 12, 12}, 1}}) {
        const Entity e = b.spawn();
        b.add<Body>(e, {bounds, layers::kCoin});
        b.add<Coin>(e, {value});
        b.add_tag(e, level::kCoinTag);
    }
    const Entity t = b.spawn();
    b.add<Body>(t, {{64, 96, 16, 16}, layers::kEnemy});
    b.add<Turret>(t);
    b.add<nnh::Node>(t, nnh::Node());
    b.add_tag(t, level::kTurretTag);
    // Spec 022: its barrel child, then the turret's hook centres its node.
    const Entity barrel = b.spawn();
    b.add<nnh::Node>(barrel, nnh::Node());
    b.add<level::Barrel>(barrel);
    b.add_tag(barrel, level::kBarrelTag);
    nnh::Hierarchy(b).attach(barrel, t, nnh::Reparent::KeepLocal);
    b.get<nnh::Node>(t)->local.position = {72, 104};
    // Turret has padding (bool, int): a copy of the stored Turret{} and the slot's own Turret{} agree on members
    // (what the replay hashes), not necessarily on padding bytes. Compare the members, then zero both objects.
    Turret* ta = a.get<Turret>(t);
    Turret* tb = b.get<Turret>(t);
    CHECK(ta != nullptr && tb != nullptr && ta->sees == tb->sees && ta->cooldown == tb->cooldown);
    if (ta == nullptr || tb == nullptr) return;
    std::memset(static_cast<void*>(ta), 0, sizeof(Turret));
    std::memset(static_cast<void*>(tb), 0, sizeof(Turret));
    CHECK(prefab_test::same_state(a, b));
}

NNH_TEST(a_sparkle_prefab_equals_the_hand_written_burst_tween_included) {
    nnh::TweenSet ta{{.tweens = 8}};
    nnh::TweenSet tb{{.tweens = 8}};
    World a = make_world(&ta);
    World b = make_world(&tb);
    level::LevelPrefabs p = make_prefabs(&ta);
    const Entity x = p.library.spawn_with(a, p.spark, nnh::Vec2{100, 50}, Velocity{{-0.75f, -1.25f}});
    // Before prefabs (Sparkles::burst): spawn, add Spark and Velocity, tag, then start the fade.
    const Entity y = b.spawn();
    Spark* spark = b.add<Spark>(y, {{100, 50}, 1.0f, {}});
    b.add<Velocity>(y, {{-0.75f, -1.25f}});
    b.add_tag(y, level::kFxTag);
    World* w = &b;
    spark->fade = tb.from_to(nnh::Setter<float>{[w, y](float alpha) {
                                 if (Spark* s = w->get<Spark>(y)) s->alpha = alpha;
                             }},
                             1.0f, 0.0f, nnh::Frames{level::kSparkFadeFrames}, {.on_finish = [w, y] { w->destroy(y); }});
    CHECK(x == y && ta.active() == 1 && tb.active() == 1);
    // The two tween sets have their own serials (TweenHandle::set): everything else must match, bit for bit.
    Spark* sa = a.get<Spark>(x);
    Spark* sb = b.get<Spark>(y);
    const auto same = [&] {
        const std::uint16_t own = sb->fade.set;
        sb->fade.set = sa->fade.set;
        const bool equal = prefab_test::same_state(a, b);
        sb->fade.set = own; // its remove hook kills the fade in its own set
        return equal;
    };
    CHECK(sa->fade.set != 0 && sb->fade.set != 0 && same());
    for (int i = 0; i < 10; ++i) {
        ta.update();
        tb.update();
    }
    CHECK(same() && sa->alpha < 1.0f);
    for (int i = 0; i < level::kSparkFadeFrames; ++i) {
        ta.update();
        tb.update();
    }
    CHECK(!a.alive(x) && !b.alive(y) && prefab_test::same_state(a, b));
}
