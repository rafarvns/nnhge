#pragma once
// Playing one level, whatever made its map (Tiled file or code): the player against the map's collision grid
// filtered by its mask (layers.hpp, spec 019), an nnh::CameraRig following it (clamped to the map), coins
// collected by a layered overlap query, doors to another demo. Its tile
// layers draw around the player: parallax layers behind, then the ground, then coins and the player, then a
// layer named "front" over them.
//
// It reads the map's objects: a point named "spawn"; objects of type "coin" (property `value`, 1 if absent);
// objects of type "door" (property `to`: "level1" or "bonus").

#include "art.hpp"
#include "coin_book.hpp"
#include "game_events.hpp"
#include "layers.hpp"
#include "player.hpp"
#include "player_sprite.hpp"
#include "replay_run.hpp"

#include <nnh/camera.hpp>
#include <nnh/tilemap.hpp>

class LevelPlay {
  public:
    static constexpr int kMaxLayers = 8;
    static constexpr int kMaxDoors = 4;

    /// `art` and `map` must outlive it.
    LevelPlay(const Art& art, const nnh::Tilemap& map);

    /// The player back at the spawn point, every coin back; publishes LevelRestarted.
    void restart();
    /// One frame with `input`: moves the player, takes coins (publishes CoinCollected), restarts after a fall
    /// into the water, moves the camera. With `doors`, walking into a door publishes ShowDemo (once).
    void update(const PlayerInput& input, bool doors);
    /// The top screen: map layers, coins, doors, the player.
    void draw() const;

    /// What the frame left, for the replay's checksum.
    ReplaySample sample() const;

  private:
    struct Door {
        nnh::Rect bounds;
        Demo to;
    };

    void read_objects();
    void take_coins();
    void enter_doors();
    void draw_doors() const;

    const Art& art_;
    const nnh::Tilemap& map_;
    nnh::collision::Grid grid_{};
    Player player_;
    PlayerSprite sprite_; ///< drawing only: an animator chooses the frames (spec 016)
    nnh::Vec2 spawn_{};
    nnh::CameraRig camera_; ///< follows the player (spec 018), see level_camera.hpp
    float look_ahead_ = 0;  ///< the camera's offset x: ahead of the player, eased when it turns
    CoinBook coins_;
    Door doors_[kMaxDoors] = {};
    int door_count_ = 0;
    bool in_door_ = false;
    int draw_layer_[kMaxLayers] = {}; ///< draw layer of each map layer
    int layer_count_ = 0;
};
