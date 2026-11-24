#pragma once
// Playing one level, whatever made its map (Tiled file or code): the player against the map's collision grid
// filtered by its mask (layers.hpp, spec 019), an nnh::CameraRig following it (clamped to the map), coins
// collected by a layered overlap query, doors to another demo. Its tile
// layers draw around the player: parallax layers behind, then the ground, then coins and the player, then a
// layer named "front" over them.
//
// The player, the coins and the turrets are entities of one nnh::World (spec 020, level_world.hpp) bound to the level's
// scene with the turrets' bullet pool: flushed before each update, emptied when the level is left. Coins, turrets and
// sparkles are the level's prefabs (spec 021, level_prefabs.hpp): the map's objects name them by type. A restart is
// World::clear() plus the same spawns (the player, then the map's objects in order: spawn_map_objects), so every slot
// is the first load's.
// A taken coin bursts into sparkles (sparkles.hpp) faded by the level's tween set, bound after the world.
//
// It reads the map's objects: a point named "spawn" and objects of type "door" (property `to`: "level1" or "bonus")
// itself; objects of type "coin" (property `value`, 1 if absent) and "turret" (turrets.hpp: they look at the player
// and shoot slow bullets moved by rays; a hit flashes the player and publishes PlayerHit) are prefabs.

#include "art.hpp"
#include "coins.hpp"
#include "game_events.hpp"
#include "layers.hpp"
#include "level_prefabs.hpp"
#include "player.hpp"
#include "player_sprite.hpp"
#include "replay_run.hpp"
#include "sparkles.hpp"
#include "turrets.hpp"

#include <nnh/camera.hpp>
#include <nnh/entities.hpp>
#include <nnh/tilemap.hpp>

class LevelPlay {
  public:
    static constexpr int kMaxLayers = 8;
    static constexpr int kMaxDoors = 4;

    /// `art` and `map` must outlive it; its world and bullets are bound to `scene` (the level's scene, its owner).
    LevelPlay(const Art& art, const nnh::Tilemap& map, nnh::Scene& scene);

    /// The player back at the spawn point, every coin back; publishes LevelRestarted.
    void restart();
    /// One frame with `input`: moves the player, takes coins (publishes CoinCollected), restarts after a fall
    /// into the water, moves the camera. With `doors`, walking into a door publishes ShowDemo (once).
    void update(const PlayerInput& input, bool doors);
    /// The top screen: map layers, coins, doors, the player.
    void draw();

    /// What the frame left, for the replay's checksum.
    ReplaySample sample();
    /// Live entities of the level's world (the HUD shows it).
    int entity_count() const { return world_.count(); }
    /// From the next restart(), start 24 px left of the level's first door (the door run, spec 023).
    void start_at_door(bool on) { start_at_door_ = on; }

  private:
    struct Door {
        nnh::Rect bounds;
        Demo to;
    };

    void read_objects();
    void spawn_things();
    void take_coins();
    void enter_doors();
    void draw_doors() const;
    void draw_turrets();
    void draw_sparkles();
    bool bind_world(nnh::Scene& scene);
    std::uint32_t handle_hash(); ///< FNV-1a of the live entities' slots, ascending

    const Art& art_;
    const nnh::Tilemap& map_;
    nnh::collision::Grid grid_{};
    Player player_;
    PlayerSprite sprite_; ///< drawing only: an animator chooses the frames (spec 016)
    nnh::Vec2 spawn_{};
    nnh::CameraRig camera_; ///< follows the player (spec 018), see level_camera.hpp
    float look_ahead_ = 0;  ///< the camera's offset x: ahead of the player, eased when it turns
    nnh::World world_;      ///< the player, coins, turrets and sparkles (level_world.hpp)
    bool world_bound_;      ///< initialised between world_ and tweens_: the world is bound before the set
    nnh::TweenSet tweens_;  ///< the sparkles' fades, bound to the level scene after the world
    level::LevelPrefabs prefabs_; ///< coin, turret, spark (level_prefabs.hpp)
    Sparkles sparkles_{world_, prefabs_};
    nnh::Entity player_entity_{};
    TurretBook turrets_{world_};
    nnh::Hierarchy tree_{world_}; ///< the turrets' barrels (spec 022): the pass runs after the turrets aim
    int coin_count_ = 0; ///< coins at the start
    int collected_ = 0;
    Door doors_[kMaxDoors] = {};
    int door_count_ = 0;
    bool in_door_ = false;
    bool start_at_door_ = false; ///< restart() puts the player 24 px left of the first door (the door run)
    int draw_layer_[kMaxLayers] = {}; ///< draw layer of each map layer
    int layer_count_ = 0;
};
