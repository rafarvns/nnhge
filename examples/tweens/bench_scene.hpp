#pragma once
// The cost of 256 active tweens (spec 015 AC-7): every kind and curve, elastic included, looping forever in
// an unbound set; update() is timed with time::now() for 600 frames (average and worst), then the scene logs
// "tweens bench: 256 tweens avg X.XX ms worst Y.YY ms (target < 1 ms) PASS|OVER". SELECT: back to the
// showcase.

#include "demos.hpp"

#include <cstddef>
#include <nnh/gfx.hpp>
#include <nnh/scene.hpp>
#include <nnh/time.hpp>
#include <nnh/tween.hpp>

class BenchScene final : public nnh::Scene {
public:
    static constexpr int kTweens = 256;
    static constexpr int kFrames = 600;

    explicit BenchScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { set_ = nnh::TweenSet{}; } // frees the block
    void update() override;
    void draw() override;

private:
    void report();

    const Demos& demos_;
    nnh::TweenSet set_;
    std::size_t set_bytes_ = 0;
    float f_[kTweens / 3 + 1] = {};
    nnh::Vec2 v_[kTweens / 3 + 1] = {};
    nnh::Color c_[kTweens / 3 + 1] = {};
    int frames_ = 0;
    nnh::Duration total_{}, worst_{};
    char result_[96] = {}; ///< the log line
    char shown_[48] = {};  ///< its short form on screen
};
