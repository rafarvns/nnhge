#include "level_prefabs.hpp"

#include <nnh/tilemap.hpp>

namespace level {
namespace {

/// A coin's value: its map object's int property `value` (1 when absent, as Tiled's levels expect).
void read_value(nnh::World& w, nnh::Entity e, const nnh::SpawnInfo& info) {
    if (info.object == nullptr) return;
    if (Coin* coin = w.get<Coin>(e)) coin->value = info.object->properties.get_int("value", 1);
}

/// A turret's node sits at its box's centre (its collision box keeps the map's rectangle: placement stays the box).
void centre_node(nnh::World& w, nnh::Entity e, const nnh::SpawnInfo&) {
    const Body* body = w.get<Body>(e);
    nnh::Node* node = w.get<nnh::Node>(e);
    if (body == nullptr || node == nullptr) return;
    node->local.position = {body->box.x + body->box.w * 0.5f, body->box.y + body->box.h * 0.5f};
}

} // namespace

nnh::Result<LevelPrefabs> create_level_prefabs(nnh::TweenSet* tweens) {
    auto made = nnh::PrefabLibrary::create({.max_prefabs = 4, .max_entries = 9, .value_bytes = 256, .max_children = 1});
    if (!made) return made.error();
    LevelPrefabs p;
    p.library = static_cast<nnh::PrefabLibrary&&>(made.value());
    const nnh::Placement box = nnh::place(&Body::box);
    auto coin = p.library.add("coin", {.tags = {kCoinTag}, .place = box, .on_spawn = read_value},
                              Body{{0, 0, 12, 12}, layers::kCoin}, Coin{1});
    auto barrel = p.library.add("barrel", {.tags = {kBarrelTag}}, nnh::Node(), Barrel{});
    if (!barrel) return barrel.error();
    auto turret = p.library.add("turret",
                                {.tags = {kTurretTag}, .place = box, .on_spawn = centre_node,
                                 .children = {nnh::child(barrel.value())}},
                                Body{{0, 0, 16, 16}, layers::kEnemy}, Turret{}, nnh::Node());
    // The fade writes alpha through a setter that captures the world and the handle (so it stops once the sparkle
    // is gone) and destroys the sparkle when it ends; Spark's remove hook kills it if the sparkle goes first.
    auto spark = p.library.add(
        "spark",
        {.tags = {kFxTag},
         .place = nnh::place(&Spark::position),
         .on_spawn =
             [tweens](nnh::World& w, nnh::Entity e, const nnh::SpawnInfo&) {
                 Spark* s = w.get<Spark>(e);
                 if (tweens == nullptr || s == nullptr) return;
                 nnh::World* world = &w;
                 s->fade = tweens->from_to(nnh::Setter<float>{[world, e](float alpha) {
                                               if (Spark* live = world->get<Spark>(e)) live->alpha = alpha;
                                           }},
                                           1.0f, 0.0f, nnh::Frames{kSparkFadeFrames},
                                           {.on_finish = [world, e] { world->destroy(e); }});
             }},
        Spark{{}, 1.0f, {}}, Velocity{});
    if (!coin || !turret || !spark) return nnh::Error::make(nnh::ErrorCode::LimitReached, "the level's prefabs");
    p.coin = coin.value();
    p.turret = turret.value();
    p.spark = spark.value();
    p.barrel = barrel.value();
    return p;
}

} // namespace level
