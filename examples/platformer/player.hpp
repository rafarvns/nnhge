#pragma once
// The player: runs, jumps, drops through one-way planks (Down + A), collides with any collision::Grid.
// Fixed steps of one frame (60 Hz) and input as a PlayerInput (buttons or a replay's script), so a run plays
// the same on every system (tested in tests/platformer/test_player.cpp). How it looks: player_sprite.hpp.

#include "player_input.hpp"

#include <nnh/collision.hpp>
#include <nnh/gfx.hpp>
#include <nnh/math.hpp>

/// This frame's input from the buttons: D-Pad/Circle Pad to run, A or B to jump, Down held to drop.
PlayerInput read_player_input();

class Player {
  public:
    /// Places the player with its feet at `feet` (standing still).
    void spawn(nnh::Vec2 feet);
    /// Moves one frame through `grid` with `input`.
    void update(const nnh::collision::Grid& grid, const PlayerInput& input);

    nnh::Rect box() const { return box_; }
    bool on_ground() const { return on_ground_; }
    nnh::Vec2 velocity() const { return velocity_; }
    bool facing_left() const { return facing_left_; }
    /// The middle of the bottom of its box: where the sprite stands (player_sprite.hpp draws it).
    nnh::Vec2 feet() const { return {box_.x + box_.w / 2, box_.y + box_.h}; }
    /// Sides the last move touched.
    nnh::collision::Sides touching() const { return touching_; }

  private:
    nnh::Rect box_{0, 0, 10, 14}; // inside the 16x16 sprite: 3 px in from each side, 2 px from the top
    nnh::Vec2 velocity_{};
    nnh::collision::Sides touching_{};
    bool on_ground_ = false;
    bool facing_left_ = false;
};
