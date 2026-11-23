#include "arena.hpp"

#include <cstdio>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace arena {
namespace {

namespace collision = nnh::collision;
using nnh::Entity;
using nnh::World;

constexpr int kExplosionFrames = 20;
constexpr float kShipSpeed = 2;
constexpr float kBulletSpeed = 3;

float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// "enemy,flying": the example's tags an entity has (the engine stores no names).
void tag_list(const World& w, Entity e, char* out, std::size_t size) {
    out[0] = '\0';
    std::size_t at = 0;
    for (nnh::Tag t : {kEnemy, kFlying, kCoin, kFrozen}) {
        if (!w.has_tag(e, t)) continue;
        const int n = std::snprintf(out + at, size - at, "%s%s", at == 0 ? "" : ",", tag_name(t));
        if (n > 0) at += static_cast<std::size_t>(n);
        if (at >= size) break;
    }
}

} // namespace

World Arena::create_world(Arena* self) {
    const nnh::ComponentDesc kinds[] = {
        nnh::component<Body>(kMaxEntities,
                             [self](World& w, Entity e, Body& b) {
                                 if (w.clearing()) return; // an effect: skipped while the scene's exit clears
                                 if (w.has_tag(e, kEnemy) || w.has_tag(e, kCoin)) self->spawn_explosion(centre(b.box));
                             }),
        nnh::component<Velocity>(kMaxEntities),
        nnh::component<Bob>(8, [self](World&, Entity, Bob& b) { self->tweens_.kill(b.tween); }),
        nnh::component<Coin>(16),
        nnh::component<Missile>(2),
        nnh::component<Explosion>(32),
    };
    auto world = World::create({.max_entities = kMaxEntities, .tags_per_entity = 3}, kinds);
    NNH_VERIFY(world.ok(), "the arena's world: out of memory");
    return static_cast<World&&>(world.value());
}

bool Arena::bind_world(nnh::Scene& scene) {
    world_.bind(scene);
    return true;
}

Arena::Arena(nnh::Scene& scene)
    : world_(create_world(this)), bound_(bind_world(scene)), tweens_(scene, {.tweens = 16}), bullets_(scene) {}

void Arena::spawn_walker(int i) {
    const Entity e = world_.spawn();
    const float x = static_cast<float>(24 + i * 60);
    world_.add<Body>(e, {{x, i % 2 == 0 ? 60.0f : 96.0f, 16, 14}, bits::kEnemy});
    world_.add<Velocity>(e, {{i % 2 == 0 ? 1.0f : -0.5f, 0}});
    world_.add_tag(e, kEnemy);
}

void Arena::spawn_flyer(int i) {
    const Entity e = world_.spawn();
    const float y = 140;
    world_.add<Body>(e, {{static_cast<float>(60 + i * 84), y, 14, 10}, bits::kEnemy});
    world_.add<Velocity>(e, {{i % 2 == 0 ? -0.5f : 0.75f, 0}});
    Bob* bob = world_.add<Bob>(e, {y, 0, {}});
    // A handle-capturing setter: writes only while the flyer lives (a destroyed one's handle is stale).
    World* w = &world_;
    bob->tween = tweens_.from_to(nnh::Setter<float>{[w, e](float v) {
                                     if (Bob* b = w->get<Bob>(e)) b->offset = v;
                                 }},
                                 -10.0f, 10.0f, nnh::Frames{40 + 8 * i},
                                 {.ease = nnh::Ease::SineInOut, .loops = nnh::kForever,
                                  .loop_mode = nnh::LoopMode::Yoyo});
    world_.add_tag(e, kEnemy);
    world_.add_tag(e, kFlying);
}

void Arena::spawn_coin(int i) {
    const Entity e = world_.spawn();
    world_.add<Body>(e, {{static_cast<float>(28 + i * 46), 180, 8, 8}, bits::kCoin});
    world_.add<Coin>(e, {1 + i % 3});
    world_.add_tag(e, kCoin);
}

void Arena::spawn_explosion(nnh::Vec2 at) {
    const Entity e = world_.spawn();
    world_.add<Explosion>(e, {at, kExplosionFrames});
    world_.add_tag(e, kFx);
}

void Arena::populate() {
    world_.clear();
    bullets_.clear();
    tweens_.kill_all();
    ship_ = world_.spawn();
    world_.add<Body>(ship_, {{193, 212, 14, 10}, bits::kShip});
    for (int i = 0; i < 6; ++i) spawn_walker(i);
    for (int i = 0; i < 4; ++i) spawn_flyer(i);
    for (int i = 0; i < 8; ++i) spawn_coin(i);
    missile_ = world_.spawn();
    world_.add<Body>(missile_, {{198, 200, 6, 6}, bits::kMissile});
    world_.add<Missile>(missile_, {world_.first(kEnemy)});
    missile_status_[0] = '\0';
    fire_misses_ = -1;
}

void Arena::move_bodies() {
    world_.each<Velocity, Body>([&](Entity e, Velocity& v, Body& b) {
        if (world_.has_tag(e, kFrozen)) return;
        b.box.x += v.v.x;
        if (b.box.x < kBounds.x || b.box.x + b.box.w > kBounds.x + kBounds.w) {
            v.v.x = -v.v.x; // bounce off the side walls
            b.box.x = clamp(b.box.x, kBounds.x, kBounds.x + kBounds.w - b.box.w);
        }
    });
    world_.each<Bob, Body>([&](Entity e, Bob& bob, Body& b) {
        if (!world_.has_tag(e, kFrozen)) b.box.y = bob.base_y + bob.offset;
    });
}

void Arena::move_missile() {
    Missile* m = world_.get<Missile>(missile_);
    Body* body = world_.get<Body>(missile_);
    if (m == nullptr || body == nullptr) return;
    if (m->target && !world_.alive(m->target)) { // the handle went stale: say so, chase the next enemy
        std::snprintf(missile_status_, sizeof missile_status_, "target lost (stale handle slot %d gen %d)",
                      m->target.slot(), m->target.generation());
        NNH_LOG_INFO(example, "%s", missile_status_);
        m->target = world_.first(kEnemy);
    }
    const Body* target = world_.get<Body>(m->target);
    if (target == nullptr) return;
    const nnh::Vec2 to = centre(target->box) - centre(body->box);
    body->box.x += to.x > 1 ? 1.0f : (to.x < -1 ? -1.0f : 0.0f);
    body->box.y += to.y > 1 ? 1.0f : (to.y < -1 ? -1.0f : 0.0f);
}

void Arena::fire(bool held) {
    if (!held) {
        if (fire_misses_ >= 0 && bullets_.stats().misses > fire_misses_)
            NNH_LOG_INFO(example, "pool: full, misses %d", bullets_.stats().misses);
        fire_misses_ = -1;
        return;
    }
    if (fire_misses_ < 0) fire_misses_ = bullets_.stats().misses;
    const Body* ship = world_.get<Body>(ship_);
    if (ship != nullptr) bullets_.acquire({{ship->box.x + ship->box.w * 0.5f, ship->box.y - 2}});
}

void Arena::move_bullets() {
    const nnh::Storage<Body> bodies = world_.storage<Body>();
    const collision::BoxView view = collision::box_view(bodies.data(), bodies.extent(), &Body::box, &Body::layers);
    for (auto [h, b] : bullets_.items()) {
        b.position.y -= kBulletSpeed;
        int hit = -1;
        if (collision::boxes_at(view, b.position, &hit, 1, {.mask = bits::kEnemy}) > 0) {
            world_.destroy(bodies.owner(hit)); // its Body hook spawns an explosion
            bullets_.release(h);
        } else if (b.position.y < kBounds.y) {
            bullets_.release(h);
        }
    }
}

void Arena::age_explosions() {
    world_.each<Explosion>([&](Entity e, Explosion& x) {
        if (--x.frames <= 0) world_.destroy(e); // destroyed inside the iteration: safe
    });
}

void Arena::update(nnh::Vec2 move, bool fire_held) {
    if (Body* ship = world_.get<Body>(ship_)) {
        ship->box.x = clamp(ship->box.x + move.x * kShipSpeed, kBounds.x, kBounds.x + kBounds.w - ship->box.w);
        ship->box.y = clamp(ship->box.y + move.y * kShipSpeed, 160, kBounds.y + kBounds.h - ship->box.h);
    }
    fire(fire_held);
    move_bodies();
    move_missile();
    move_bullets();
    age_explosions();
}

void Arena::destroy_at(nnh::Vec2 point) {
    const nnh::Storage<Body> bodies = world_.storage<Body>();
    const collision::BoxView view = collision::box_view(bodies.data(), bodies.extent(), &Body::box, &Body::layers);
    int found = -1;
    if (collision::boxes_at(view, point, &found, 1, {.mask = bits::kEnemy | bits::kCoin}) == 0) {
        NNH_LOG_INFO(example, "touch: nothing at %d,%d", static_cast<int>(point.x), static_cast<int>(point.y));
        return;
    }
    const Entity e = bodies.owner(found);
    char tags[48];
    tag_list(world_, e, tags, sizeof tags);
    NNH_LOG_INFO(example, "destroyed: slot %d gen %d tags %s", e.slot(), e.generation(), tags);
    world_.destroy(e);
}

void Arena::zap_target() {
    const Missile* m = world_.get<Missile>(missile_);
    if (m == nullptr || !world_.alive(m->target)) return;
    NNH_LOG_INFO(example, "zap: slot %d gen %d", m->target.slot(), m->target.generation());
    world_.destroy(m->target);
}

int Arena::freeze(nnh::Tag t) {
    int n = 0;
    world_.each_tagged(t, [&](Entity e) { n += world_.add_tag(e, kFrozen) ? 1 : 0; });
    return n;
}

int Arena::unfreeze() {
    int n = 0;
    world_.each_tagged(kFrozen, [&](Entity e) { n += world_.remove_tag(e, kFrozen) ? 1 : 0; });
    return n;
}

} // namespace arena
