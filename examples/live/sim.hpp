#pragma once
// The example's little world (spec 030 AC-1, AC-2): a jumping square and a spawner of bouncing balls, stepped one 60 Hz
// frame at a time from the tuning values. Pure (no engine calls), built without fused multiply-add, so the self-check's
// sum is the same on every compiler and on the 3DS.

#include <cstdint>

#include <nnh/gfx.hpp>
#include <nnh/math.hpp>

inline constexpr float kGroundY = 200.0f;
inline constexpr int kMaxBalls = 32;

enum class Shape : std::uint8_t { Circle, Square, Diamond };
enum class PlayerState : std::uint8_t { Idle, Run, Jump, Fall };

/// Every value the developer tunes (the defaults are the declared values: Release keeps them).
struct Tuning {
    float jump = 5.5f;     ///< player/jump: 0..12 step 0.25
    float gravity = 0.3f;  ///< player/gravity: 0.05..1 step 0.01
    float run = 1.75f;     ///< player/run: 0.25..4 step 0.25
    bool hitboxes = false; ///< debug/hitboxes
    int count = 6;         ///< spawn/count: 0..32
    Shape shape = Shape::Circle;
    nnh::Color sky{70, 120, 200, 255};
    nnh::Color ground{60, 150, 70, 255};
    float dust = 12;       ///< fx/dust: 0..64 step 1 (the dust burst's size on landing)
};

struct Ball {
    nnh::Vec2 pos;
    nnh::Vec2 vel;
};

struct World {
    nnh::Vec2 pos{40, kGroundY};
    nnh::Vec2 vel;
    PlayerState state = PlayerState::Idle;
    bool on_ground = true;
    int landings = 0;
    Ball balls[kMaxBalls];
    int ball_count = 0;
    std::uint32_t frame = 0;
};

/// One frame: `dir` −1, 0 or 1; `jump` pressed this frame.
void sim_step(World& w, const Tuning& t, int dir, bool jump);

/// The balls the spawner keeps (the first `count`), placed deterministically.
void sim_respawn(World& w, const Tuning& t);

const char* state_name(PlayerState s);
