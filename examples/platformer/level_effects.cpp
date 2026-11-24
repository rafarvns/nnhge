#include "level_effects.hpp"

#include <nnh/assert.hpp>
#include <nnh/gfx.hpp>

namespace {

using nnh::Frames;
using nnh::ParticleDesc;

nnh::ParticleEmitter make(const ParticleDesc& d, int capacity) {
    auto e = nnh::ParticleEmitter::create(d, capacity);
    NNH_VERIFY(e.ok(), "the level's effects: out of memory");
    return static_cast<nnh::ParticleEmitter&&>(e.value());
}

ParticleDesc dust_desc() { // grey puffs kicked up from the ground
    ParticleDesc d;
    d.shape = nnh::ParticleShape::Box;
    d.box = {8, 1};
    d.angle = -nnh::kQuarterTurn;
    d.spread = 2.4f;
    d.speed = {20, 55};
    d.gravity = {0, 160};
    d.damping = 3;
    d.lifetime = {Frames{12}, Frames{24}};
    d.color_min = nnh::rgba(170, 160, 140, 220);
    d.color_max = nnh::rgba(220, 210, 190, 255);
    d.color_keys[0] = {0, nnh::colors::white};
    d.color_keys[1] = {1, nnh::rgba(255, 255, 255, 0)};
    d.size = {2, 3};
    d.seed = 0xD057u;
    return d;
}

ParticleDesc puff_desc() { // a small cone along local +x (a wall's normal)
    ParticleDesc d;
    d.spread = 1.8f;
    d.speed = {25, 70};
    d.damping = 6;
    d.lifetime = {Frames{8}, Frames{18}};
    d.color_min = nnh::rgba(255, 220, 120, 255);
    d.color_max = nnh::rgba(255, 250, 200, 255);
    d.color_keys[0] = {0.3f, nnh::colors::white};
    d.color_keys[1] = {1, nnh::rgba(255, 120, 60, 0)};
    d.size = {1, 2};
    d.seed = 0x9FFu;
    return d;
}

ParticleDesc flash_desc() { // a short, bright burst out of a barrel
    ParticleDesc d;
    d.spread = 0.5f;
    d.speed = {60, 130};
    d.lifetime = {Frames{5}, Frames{9}};
    d.color_min = nnh::rgba(255, 255, 220, 255);
    d.color_max = nnh::rgba(255, 230, 120, 255);
    d.color_keys[0] = {0, nnh::colors::white};
    d.color_keys[1] = {1, nnh::rgba(255, 160, 60, 0)};
    d.size = {1, 2};
    d.seed = 0xF1A5u;
    return d;
}

/// The rotation of an axis-aligned wall normal (raycasts give one of four).
float normal_angle(nnh::Vec2 n) {
    if (n.x > 0) return 0;
    if (n.x < 0) return nnh::kHalfTurn;
    return n.y > 0 ? nnh::kQuarterTurn : -nnh::kQuarterTurn;
}

nnh::WorldTransform2D at(nnh::Vec2 position, float rotation) {
    return nnh::to_world({.position = position, .rotation = rotation});
}

} // namespace

LevelEffects::LevelEffects() : dust_(make(dust_desc(), 64)), puff_(make(puff_desc(), 64)), flash_(make(flash_desc(), 32)) {}

void LevelEffects::update(const EffectsInput& in) {
    emit_player(in.player);
    emit_bullets(in.turrets, in.player);
    emit_turrets(in.world, in.tree);
    dust_.update();
    puff_.update();
    flash_.update();
}

void LevelEffects::emit_player(const Player& p) {
    const bool on_ground = p.on_ground();
    const float vx = p.velocity().x;
    const bool running = on_ground && (vx > kRunSpeed || vx < -kRunSpeed);
    if (on_ground && !was_on_ground_) {
        dust_.emit_at(p.feet(), kLandingDust);
    } else if (running && !was_running_) {
        const nnh::Vec2 f = p.feet();
        dust_.emit_at(nnh::Vec2{f.x + (vx > 0 ? -5.0f : 5.0f), f.y}, kRunDust); // behind the feet
    }
    was_on_ground_ = on_ground;
    was_running_ = running;
}

void LevelEffects::emit_bullets(TurretBook& turrets, const Player& p) {
    for (auto [h, b] : turrets.bullets().items()) {
        if (b.stuck == TurretBook::kStuckFrames) puff_.emit_at(at(b.position, normal_angle(b.normal)), kWallPuff);
    }
    if (turrets.hits() > hits_) { // the player was hit: a ring of four small cones at its centre
        const nnh::Rect r = p.box();
        const nnh::Vec2 c{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
        for (int k = 0; k < 4; ++k) puff_.emit_at(at(c, static_cast<float>(k) * nnh::kQuarterTurn), kHitPuff / 4);
    }
    hits_ = turrets.hits();
}

void LevelEffects::emit_turrets(nnh::World& world, const nnh::Hierarchy& tree) {
    world.each<level::Turret>([&](nnh::Entity e, level::Turret& t) {
        if (t.cooldown != TurretBook::kFirePeriod) return; // fired this frame
        const nnh::Node* barrel = world.get<nnh::Node>(tree.first_child(e));
        if (barrel == nullptr) return;
        const nnh::WorldTransform2D& w = barrel->world();
        flash_.emit_at(at(nnh::transform_point(w.map, TurretBook::kMuzzle), w.rotation), kMuzzleFlash);
    });
}

void LevelEffects::clear() {
    dust_.clear();
    puff_.clear();
    flash_.clear();
    was_on_ground_ = true;
    was_running_ = false;
    hits_ = 0;
}

void LevelEffects::draw(int layer) const {
    dust_.draw(nnh::Screen::Top, {.layer = layer});
    puff_.draw(nnh::Screen::Top, {.layer = layer});
    flash_.draw(nnh::Screen::Top, {.layer = layer});
}
