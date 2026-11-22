#pragma once
// The libraries the animation example builds in code (spec 016): the gallery strip (8 numbered 16x16 frames, one clip
// per mode, markers) and a knight state machine on a 13-frame grid. Pure: the self-check, the bench and the unit
// tests use them with no assets.

#include <nnh/animation.hpp>

/// The gallery: clips loop, once, repeat3, pingpong, ppr2 (ping-pong reverse, 2 round trips), reverse, each of the 8
/// frames at 6 frames each; `tick` markers on frames 0 and 4 of every clip (value = the frame).
nnh::SpriteLibrary make_gallery_library();
inline constexpr int kGalleryClips = 6;
inline constexpr const char* kGalleryNames[kGalleryClips] = {"loop", "once", "repeat3", "pingpong", "ppr2",
                                                               "reverse"};

/// The knight in code: 13 frames of 32x32 in a grid; clips idle, run (`step` on 0 and 2), run_shoot, jump, fall,
/// attack (`hit` 2 on 1), hurt; states with the same names; jump/attack/hurt finish into fall/idle/idle; attack can't
/// be cut short except by hurt; run <-> run_shoot keep their time; hurt waits for its end.
nnh::SpriteLibrary make_knight_library();

/// The knight's states, found by name in a library that has them (code-built or the loaded hero with its states
/// added by add_knight_states()).
struct KnightStates {
    nnh::SpriteStateId idle, run, run_shoot, jump, fall, attack, hurt;
    explicit KnightStates(const nnh::SpriteLibrary& lib);
};

/// Adds the knight's states and rules to `lib` (which must have the clips idle, run, jump, fall, attack, hurt and
/// room for 6 states and 4 rules): how the loaded Aseprite hero gets its state machine (FR-18).
bool add_knight_states(nnh::SpriteLibrary& lib);
