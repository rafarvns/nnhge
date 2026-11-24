#include "level_world.hpp"

namespace level {

nnh::Result<nnh::World> create_world(nnh::TweenSet* tweens) {
    const nnh::ComponentDesc kinds[] = {
        nnh::component<Body>(kMaxEntities),
        nnh::component<Coin>(kMaxCoins),
        nnh::component<Turret>(kMaxTurrets),
        // A sparkle that goes before its fade ends (a restart's clear) takes its tween with it: nothing keeps
        // running for a slot that is free.
        nnh::component<Spark>(kMaxSparks,
                              [tweens](nnh::World&, nnh::Entity, Spark& s) {
                                  if (tweens != nullptr) tweens->kill(s.fade);
                              }),
        nnh::component<Velocity>(kMaxSparks),
        nnh::node_component(kMaxNodes), // spec 022: turrets and their barrels
        nnh::component<Barrel>(kMaxNodes),
    };
    return nnh::World::create({.max_entities = kMaxEntities, .tags_per_entity = 2}, kinds);
}

} // namespace level
