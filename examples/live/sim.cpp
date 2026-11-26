// The example's world (see sim.hpp). Built with -ffp-contract=off.

#include "sim.hpp"

namespace {

void step_ball(Ball& b, float gravity) {
    b.vel.y += gravity;
    b.pos.x += b.vel.x;
    b.pos.y += b.vel.y;
    if (b.pos.y >= kGroundY) {
        b.pos.y = kGroundY;
        b.vel.y = -b.vel.y * 0.875f;
    }
    if (b.pos.x < 8 || b.pos.x > 392) {
        b.vel.x = -b.vel.x;
        b.pos.x = b.pos.x < 8 ? 8 : 392;
    }
}

} // namespace

void sim_respawn(World& w, const Tuning& t) {
    w.ball_count = t.count < 0 ? 0 : (t.count > kMaxBalls ? kMaxBalls : t.count);
    for (int i = 0; i < w.ball_count; ++i) {
        w.balls[i].pos = {static_cast<float>(60 + 40 * (i % 8)), static_cast<float>(40 + 12 * (i / 8))};
        w.balls[i].vel = {static_cast<float>((i % 3) - 1) * 0.75f, 0};
    }
}

void sim_step(World& w, const Tuning& t, int dir, bool jump) {
    ++w.frame;
    w.vel.x = static_cast<float>(dir) * t.run;
    if (jump && w.on_ground) {
        w.vel.y = -t.jump;
        w.on_ground = false;
    }
    w.vel.y += t.gravity;
    if (w.vel.y > 8) w.vel.y = 8;
    w.pos.x += w.vel.x;
    w.pos.y += w.vel.y;
    if (w.pos.x < 8) w.pos.x = 8;
    if (w.pos.x > 392) w.pos.x = 392;
    if (w.pos.y >= kGroundY) {
        if (!w.on_ground) ++w.landings;
        w.pos.y = kGroundY;
        w.vel.y = 0;
        w.on_ground = true;
    }
    w.state = !w.on_ground ? (w.vel.y < 0 ? PlayerState::Jump : PlayerState::Fall)
                           : (dir != 0 ? PlayerState::Run : PlayerState::Idle);
    if (w.ball_count != t.count) sim_respawn(w, t);
    for (int i = 0; i < w.ball_count; ++i) step_ball(w.balls[i], t.gravity);
}

const char* state_name(PlayerState s) {
    switch (s) {
    case PlayerState::Idle: return "idle";
    case PlayerState::Run: return "run";
    case PlayerState::Jump: return "jump";
    case PlayerState::Fall: return "fall";
    }
    return "?";
}
