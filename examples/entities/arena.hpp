#pragma once
// The entities example's arena (spec 020 AC-5): a world of walkers (Body + Velocity, "enemy"), flyers (+ a Bob driven
// by a tween through a handle-capturing setter, "enemy" + "flying"), coins ("coin"), the ship and a homing missile,
// plus the ship's bullets in a pool of 32. The world, then the tween set, then the pool are bound to the showcase
// scene (D30): flushed before its update, emptied when it leaves (the world first, so its hooks see live tweens).
// Body's remove hook spawns an explosion (tag "fx") where an enemy or a coin was, unless the world is clearing.

#include "components.hpp"
#include "tags.hpp"

#include <nnh/entities.hpp>
#include <nnh/pool.hpp>
#include <nnh/scene.hpp>
#include <nnh/tween.hpp>

namespace arena {

inline constexpr nnh::Rect kBounds{8, 20, 384, 212}; ///< the arena's inside, in world pixels (the top screen)
inline constexpr int kMaxEntities = 64;
inline constexpr int kBullets = 32;

class Arena {
  public:
    using Bullets = nnh::Pool<Bullet, kBullets>;

    /// Binds the world, the tween set and the bullets to `scene` (its owner).
    explicit Arena(nnh::Scene& scene);

    /// Everything back to the first layout: clear() and the same spawns (ship, 6 walkers, 4 flyers, 8 coins, missile).
    void populate();
    /// One frame: the ship moves by `move` (pixels), fires while `fire` is held, then every system runs.
    void update(nnh::Vec2 move, bool fire);

    /// Destroys the enemy or coin under `point` (world pixels; spec 019 boxes_at over the body storage) and logs
    /// `destroyed: slot n gen g tags ...`, or `touch: nothing at x,y`.
    void destroy_at(nnh::Vec2 point);
    /// Destroys the missile's target (B): the missile finds its handle stale on its next move.
    void zap_target();
    /// Entities tagged `t` into `out` (ascending slots); returns the total.
    int find(nnh::Tag t, nnh::Entity* out, int capacity) const { return world_.find(t, out, capacity); }
    /// Tags every entity tagged `t` "frozen" (each_tagged): the movement skips them. Returns how many.
    int freeze(nnh::Tag t);
    /// Removes "frozen" from every entity. Returns how many.
    int unfreeze();

    nnh::World& world() { return world_; }
    Bullets& bullets() { return bullets_; }
    nnh::Entity missile() const { return missile_; }
    /// The missile's last stale target ("target lost (stale handle slot n gen g)"), or "" before any.
    const char* missile_status() const { return missile_status_; }

  private:
    static nnh::World create_world(Arena* self);
    bool bind_world(nnh::Scene& scene);
    void spawn_walker(int i);
    void spawn_flyer(int i);
    void spawn_coin(int i);
    void spawn_explosion(nnh::Vec2 at);
    void move_bodies();
    void move_missile();
    void move_bullets();
    void age_explosions();
    void fire(bool held);

    nnh::World world_;     // bound first (by bound_): its hooks run before the tween set is cleared
    bool bound_;           // initialised between world_ and tweens_: binds the world before the set binds itself
    nnh::TweenSet tweens_; // bound second
    Bullets bullets_;      // bound third
    nnh::Entity ship_{};
    nnh::Entity missile_{};
    int fire_misses_ = -1; ///< misses when the fire button went down (-1: not held)
    char missile_status_[64] = {};
};

/// Body's centre.
inline nnh::Vec2 centre(nnh::Rect r) { return {r.x + r.w * 0.5f, r.y + r.h * 0.5f}; }

} // namespace arena
