#pragma once
// The platformer's turrets (spec 019 AC-4 step 2; spec 020 AC-4 step 1), pure (collision queries and the level's world:
// tested in tests/platformer/test_turrets.cpp). Turrets are entities of the level's world (a Body on layers::kEnemy and
// a level::Turret, tagged "turret"); bullets come from a pool of 16 (the lowest free slot, as the old "first inactive
// bullet"). Each frame a turret looks at the player (line_of_sight from its centre to the player's, through the
// level's grid with one-way planks ignored, within a range box); while it sees the player it fires a slow bullet every
// 90 frames in one of 8 fixed directions (short binary fractions: exact sums, no sqrt). A bullet moves by a ray each
// frame against the level's grid and the world's body storage, read in place through a box view and filtered by the
// bullets' mask (layers::kBulletMask: the player's body only), so turrets and coins never stop it: it stops flush on
// the first wall it meets however fast it goes, stays there 45 frames, then goes; one reaching the player (or that the
// player ran into: a start inside counts) counts a hit and flashes the player for 20 frames. Bullets never move the
// player.

#include "level_world.hpp"

#include <cstdint>
#include <nnh/collision.hpp>
#include <nnh/pool.hpp>

class TurretBook {
  public:
    static constexpr int kMaxBullets = 16;
    static constexpr int kFirePeriod = 90;
    static constexpr int kStuckFrames = 45;
    static constexpr int kFlashFrames = 20;
    static constexpr float kRangeX = 192, kRangeY = 120; ///< half-sizes of the box a turret sees in

    struct Bullet {
        nnh::Vec2 position{};
        nnh::Vec2 velocity{};
        nnh::Vec2 normal{}; ///< of the wall it stopped at
        int stuck = 0;      ///< frames left on a wall; 0 while flying
    };
    using Bullets = nnh::Pool<Bullet, kMaxBullets>;

    /// `world` (a level::create_world() world) must outlive the book.
    explicit TurretBook(nnh::World& world) : world_(world) {}

    /// Spawns a turret (its box: a 16 x 16 cell). Null when the level already has level::kMaxTurrets.
    static nnh::Entity spawn_turret(nnh::World& world, nnh::Rect box);
    /// Back to the start: no bullets, no hits (the turrets themselves are spawned again by the level's restart).
    void restart();
    /// One frame: sight, firing, bullets moved through `grid` and the world's bodies. `player` is the player's entity
    /// (its Body is the target). Flushes the bullet pool first (a no-op when it is bound to a scene).
    void update(const nnh::collision::Grid& grid, nnh::Entity player);
    /// Fires a bullet from `from` moving `velocity` per frame (what a turret does; tests use other speeds). False
    /// when every bullet is in flight.
    bool shoot(nnh::Vec2 from, nnh::Vec2 velocity);

    Bullets& bullets() { return bullets_; }
    int hits() const { return hits_; }
    int flash() const { return flash_; } ///< frames left of the player's flash
    /// The turrets' state as one word (FNV-1a over hits, flash, each turret's sight and cooldown in turret order, each
    /// bullet's position bits and stuck frames in slot order): the replay's checksum takes it, so the console must
    /// reproduce every ray.
    std::uint32_t state();
    /// The bullets' velocity toward `delta` (from a turret to the player): 8 directions, 2 px per frame on an axis.
    static nnh::Vec2 aim(nnh::Vec2 delta);

  private:
    void look(const nnh::collision::Grid& grid, nnh::Vec2 target);
    void move_bullets(const nnh::collision::Grid& grid, nnh::Entity player);

    nnh::World& world_;
    Bullets bullets_;
    int hits_ = 0;
    int flash_ = 0;
};
