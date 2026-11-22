#pragma once
// The camera bench (spec 018 AC-7): each frame, 16 times, update and apply two rigs — a tall-world one and a
// bottom-screen one, both with a dead zone, smoothing and trauma — for 600 frames, then log
// "camera: bench avg X ms worst Y ms per pair (target < 0.05 ms) PASS|FAIL". SELECT goes back to the
// playground.

#include "demos.hpp"

#include <nnh/camera.hpp>
#include <nnh/scene.hpp>
#include <nnh/time.hpp>

class BenchScene final : public nnh::Scene {
  public:
    static constexpr int kFrames = 600;
    static constexpr int kPairsPerFrame = 16;

    explicit BenchScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void update() override;
    void draw() override;

  private:
    void report();

    const Demos& demos_;
    nnh::CameraRig tall_;
    nnh::CameraRig bottom_;
    int frames_ = 0;
    nnh::Duration total_{}, worst_{};
    char result_[128] = {};
    char shown_[128] = {};
};
