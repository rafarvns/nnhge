#pragma once
// The showcase (spec 030 AC-1): the world on the top screen, its tunables and watches on the overlay's pages. D-Pad
// left/right runs, A jumps, Y runs the self-check, X the bench, START quits. LIVE_SHOT sets the overlay for
// screenshots (stats, tunables, watches, colour, paused).

#include <cstdint>

#include <nnh/scene.hpp>

#include "bench.hpp"
#include "sim.hpp"
#include "tuning_handles.hpp"

class LiveScene : public nnh::Scene {
public:
    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    static void respawn(void* self);
    void self_check();

    Tuning tuning_;
    World world_;
    LiveHandles handles_;
    const char* state_text_ = "idle";
    std::uint32_t frames_ = 0;
    int shot_frames_ = 0;
    LiveBench bench_;
};
