#pragma once
// How the player looks (spec 016 AC-8): a sprite library built in code (art/player.png: 4 frames of 16x16, pivot at
// the feet) with clips idle, run (frames 1 2, 8 frames each, a `step` marker on both), jump and fall, and states with
// the same names. Each frame update() requests `on_ground ? (moving ? run : idle) : (vy < 0 ? jump : fall)`, sets the
// flip from the facing and advances the animator; a `step` notice starts a 6-frame dust puff at the feet. Drawing
// only: the player's physics, the replay and the scripts don't depend on it.

#include <nnh/animation.hpp>
#include <nnh/gfx.hpp>

/// What the sprite needs to know about the player each frame.
struct PlayerLook {
    bool on_ground = false;
    nnh::Vec2 velocity{};
    bool facing_left = false;
};

class PlayerSprite {
  public:
    PlayerSprite(); ///< builds the library (one block of 600 bytes) and starts standing
    PlayerSprite(const PlayerSprite&) = delete;
    PlayerSprite& operator=(const PlayerSprite&) = delete;

    void update(const PlayerLook& look);
    /// The player at `feet` (the middle of the bottom of its box), with the dust puff after a step.
    void draw(nnh::Screen screen, const nnh::Texture& sheet, nnh::Vec2 feet, int layer) const;

    nnh::DrawOptions draw_options() const { return anim_.draw_options(); }
    int frame() const { return anim_.frame(); }
    const char* state_name() const { return library_.state_name(anim_.state()); }
    bool stepped() const { return stepped_; } ///< a footstep in the last update
    int dust() const { return dust_; }        ///< frames left of the dust puff

  private:
    nnh::SpriteLibrary library_;
    nnh::SpriteAnimator anim_;
    nnh::SpriteStateId idle_, run_, jump_, fall_;
    int dust_ = 0;
    bool stepped_ = false;
};
