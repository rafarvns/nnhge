#pragma once
// The bench (spec 028 AC-6): bench.hpp's workloads and the options screen (hand_options.hpp) timed, 300 frames each,
// average and worst of their own time less the timer's, the draws with their share of gfx submit; then the options
// screen with 300 game sprites for 600 frames. Lines "ui bench: <name> avg X ms worst Y ms (target < Z ms) PASS|FAIL",
// the list ratio, the draw ratio, the frame line, the memory line, then "ui bench: PASS" or "ui bench: FAIL (<names>)".
// A Debug build logs "not judged (Debug)". X or UI_DEMO=bench; SELECT back.

#include <memory>
#include <nnh/scene.hpp>
#include <nnh/texture.hpp>

#include "bench.hpp"
#include "demos.hpp"
#include "hand_options.hpp"

class BenchScene final : public nnh::Scene {
public:
    enum Phase { kBase, kIdle, kMove, kGeo, kRelayout, kList10000, kList20, kDrawUi, kDrawHand, kFrame, kPhases };
    static constexpr int kFrames = 300;
    static constexpr int kFrameTestFrames = 600;

    explicit BenchScene(Demo& demo) : demo_(demo) {}
    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    struct Sum {
        double own = 0, own_worst = 0, submit = 0;
        int n = 0;
    };
    double cost(Phase p) const;
    void finish();
    void verdict();

    Demo& demo_;
    std::unique_ptr<UiBench> bench_;
    std::unique_ptr<OptionsScreen> screen_;
    Sum sums_[kPhases];
    int phase_ = 0, frame_ = 0, missed_ = 0, draws_ui_ = 0;
    double worst_frame_ = 0;
    char failed_[96] = {};
    bool done_ = false;
};
