#include "sparkles.hpp"

namespace {

// Up and outwards, in short binary fractions (the sums stay exact on every system).
constexpr nnh::Vec2 kVelocities[Sparkles::kPerCoin] = {{-0.75f, -1.25f}, {0.75f, -1.25f}, {-0.375f, -1.75f},
                                                       {0.375f, -1.75f}};

} // namespace

void Sparkles::burst(nnh::Vec2 at) {
    for (const nnh::Vec2& v : kVelocities) prefabs_.library.spawn_with(world_, prefabs_.spark, at, level::Velocity{v});
}

void Sparkles::update() {
    world_.each<level::Spark, level::Velocity>(
        [](nnh::Entity, level::Spark& s, level::Velocity& v) { s.position = s.position + v.v; });
}
