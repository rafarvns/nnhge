#pragma once
// The actions bench (spec 017 AC-9): a second set of 64 actions x 4 mixed slots, evaluated by every frame for 600
// frames; the profiler's `actions` scope (both sets) gives the average and the worst frame. On the console the
// average must stay under 0.1 ms. Then the self-check verdict is logged and shown (B goes back). Development builds
// (the profiler is compiled out of Release).

#include "shared.hpp"

#include <nnh/time.hpp>

class BenchScene final : public nnh::Scene {
public:
    explicit BenchScene(Shared& shared) : s_(shared) {}

    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    static constexpr int kFrames = 600;

    void report();

    Shared& s_;
    nnh::ActionSet bench_;
    int frames_ = 0;
    nnh::Duration total_{};
    nnh::Duration worst_{};
    bool measured_ = false;
    char result_[96] = {};  ///< the log line
    char shown_[48] = {};   ///< the numbers, short enough for the screen
    char verdict_[64] = {}; ///< the self-check's line
};
