#pragma once
// The bottom screen of the showcase: the knight loaded from the Aseprite export (art/hero.ase.json, converted by the
// build), its state machine added in code (FR-18) and driven by the game's conditions every frame: D-Pad runs, A
// jumps, B attacks (can't be cut short except by hurt), X hurts. `@step` markers play a footstep and flash a dot under
// the foot; `@hit` flashes the hit box. HERO_DEMO_RUN=1 makes it run right on its own (screenshots).

#include <nnh/animation.hpp>
#include <nnh/audio.hpp>
#include <nnh/texture.hpp>

#include "libraries.hpp"

class Hero {
public:
    bool load();   ///< texture, library (+ states), sound; false when an asset is missing
    void unload(); ///< before the engine shuts down
    void update(); ///< input, physics, state request, one animator update
    void draw() const;
    const nnh::SpriteAnimator& animator() const { return anim_; }

private:
    void on_notice(const nnh::SpriteNotice& n);

    nnh::Texture texture_;
    nnh::SpriteLibrary library_;
    nnh::Sound step_;
    KnightStates states_{nnh::SpriteLibrary{}};
    nnh::SpriteAnimator anim_;
    float x_ = 160, y_ = 200, vy_ = 0;
    bool facing_left_ = false, auto_run_ = false;
    int dust_ = 0, hit_ = 0, steps_ = 0;
};
