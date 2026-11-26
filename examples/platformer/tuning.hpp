#pragma once
// The platformer's movement and camera numbers as live values (spec 030 Q10): the defaults are the values the game
// always had (the replay lines prove them exact); in Debug/Development the level scene registers them, so the Tunables
// page tunes the jump while you play. Release keeps the defaults.

#include <nnh/live.hpp>

struct PlatformerTuning {
    float run_speed = 1.75f;       ///< player/run: px per frame
    float gravity = 0.3f;          ///< player/gravity
    float jump_speed = 5.6f;       ///< player/jump
    float max_fall = 6.0f;         ///< player/max_fall
    float look_ahead = 32;         ///< camera/look_ahead: px ahead of the player
    float look_ahead_speed = 1;    ///< camera/look_ahead_speed: px per frame
};

/// The values the player and the camera read.
extern PlatformerTuning g_tuning;

struct TuningHandles {
    nnh::live::Handle run, gravity, jump, max_fall, look_ahead, look_ahead_speed;
};

/// Registers g_tuning (the level scene's on_enter); the handles go with the scene.
void register_tuning(TuningHandles& h);
