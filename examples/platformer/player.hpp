#pragma once
// The player: runs, jumps, drops through one-way planks (Down + A), collides with any collision::Grid.
// Fixed steps of one frame (60 Hz), so a run plays the same on every system.

#include <nnh/collision.hpp>
#include <nnh/gfx.hpp>
#include <nnh/math.hpp>

class Player {
  public:
    /// Places the player with its feet at `feet` (standing still).
    void spawn(nnh::Vec2 feet);
    /// Reads input (D-Pad/Circle Pad, A/B jump, Down + A drop) and moves through `grid`.
    void update(const nnh::collision::Grid& grid);
    void draw(nnh::Screen screen, const nnh::Texture& sheet, int layer) const;

    nnh::Rect box() const { return box_; }
    bool on_ground() const { return on_ground_; }

  private:
    nnh::Rect box_{0, 0, 10, 14}; // inside the 16x16 sprite: 3 px in from each side, 2 px from the top
    nnh::Vec2 velocity_{};
    bool on_ground_ = false;
    bool facing_left_ = false;
    int walk_frames_ = 0;
};
