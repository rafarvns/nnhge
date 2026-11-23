#include "sparkles.hpp"

namespace {

// Up and outwards, in short binary fractions (the sums stay exact on every system).
constexpr nnh::Vec2 kVelocities[Sparkles::kPerCoin] = {{-0.75f, -1.25f}, {0.75f, -1.25f}, {-0.375f, -1.75f},
                                                       {0.375f, -1.75f}};

} // namespace

void Sparkles::burst(nnh::Vec2 at) {
    for (const nnh::Vec2& v : kVelocities) {
        const nnh::Entity e = world_.spawn();
        level::Spark* spark = world_.add<level::Spark>(e, {at, 1.0f, {}});
        world_.add<level::Velocity>(e, {v});
        world_.add_tag(e, level::kFxTag);
        if (spark == nullptr) continue;
        nnh::World* w = &world_;
        spark->fade = tweens_.from_to(nnh::Setter<float>{[w, e](float alpha) {
                                          if (level::Spark* s = w->get<level::Spark>(e)) s->alpha = alpha;
                                      }},
                                      1.0f, 0.0f, nnh::Frames{kFadeFrames}, {.on_finish = [w, e] { w->destroy(e); }});
    }
}

void Sparkles::update() {
    world_.each<level::Spark, level::Velocity>(
        [](nnh::Entity, level::Spark& s, level::Velocity& v) { s.position = s.position + v.v; });
}
