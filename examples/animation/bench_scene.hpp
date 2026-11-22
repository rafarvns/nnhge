#pragma once
// The cost of 512 animators (spec 016 AC-7): half on the gallery library (every clip mode, markers), half on the
// knight's state machine with a request every frame; updated inside NNH_PROFILE_SCOPE("animation bench") and timed
// with time::now() over 600 frames, then "animation bench: 512 animators avg X.XXX ms worst Y.YYY ms (target
// < 0.8 ms) PASS|OVER" is logged. SELECT: back to the showcase.

#include "demos.hpp"
#include "libraries.hpp"

#include <memory>
#include <nnh/animation.hpp>
#include <nnh/scene.hpp>
#include <nnh/texture.hpp>
#include <nnh/time.hpp>

class BenchScene final : public nnh::Scene {
public:
    static constexpr int kAnimators = 512;
    static constexpr int kFrames = 600;

    explicit BenchScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    void report();

    const Demos& demos_;
    nnh::SpriteLibrary gallery_, knight_;
    nnh::Texture texture_;
    std::unique_ptr<KnightStates> states_;
    std::unique_ptr<nnh::SpriteAnimator[]> animators_; // 512 × 64 B on the 3DS: on the heap, made on enter
    int frames_ = 0, markers_ = 0;
    nnh::Duration total_{}, worst_{};
    char result_[112] = {};
    char shown_[64] = {};
};
