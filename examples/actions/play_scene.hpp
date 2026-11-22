#pragma once
// The playfield (spec 017 AC-7): a square moved by `move` (Circle Pad or D-Pad), jumping on `jump`, charging on
// `attack` (a bar from held_frames, a shot on release), aiming with `aim`; an on-screen attack button on the bottom
// screen; START opens the controls screen. Every action edge is logged ("actions: jump pressed").

#include "shared.hpp"

#include <nnh/events.hpp>
#include <nnh/math.hpp>

class PlayScene final : public nnh::Scene {
public:
    explicit PlayScene(Shared& shared) : s_(shared) {}

    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    void on_edge(std::uint16_t set, std::uint16_t action, bool pressed);

    Shared& s_;
    nnh::Subscription pressed_;
    nnh::Subscription released_;
    nnh::Vec2 pos_{200, 200};
    float vy_ = 0;
    std::uint32_t charge_ = 0;
    nnh::Vec2 shot_{-100, -100};
    float shot_speed_ = 0;
};
