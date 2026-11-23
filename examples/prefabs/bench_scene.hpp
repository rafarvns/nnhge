#pragma once
// The prefabs bench (spec 021 AC-7): bench.hpp's workloads timed over 300 frames each with the clock, one phase after
// the other, one log line per phase: "prefabs bench: <name> avg X ms worst Y ms (target < Z ms) PASS|FAIL" (spawn
// alternates prefab and hand-written frames and adds "hand H ms ratio R"; a Debug build: "not judged (Debug)"), then
// "prefabs bench: PASS" or "prefabs bench: FAIL (<names>)" in the next frame (check.nnhs fails on it, after every number
// is logged). SELECT goes back to the showcase.

#include "bench.hpp"
#include "demos.hpp"

#include <memory>
#include <nnh/scene.hpp>
#include <nnh/time.hpp>

class BenchScene final : public nnh::Scene {
  public:
    static constexpr int kFrames = 300;
    static constexpr int kPhases = 4;

    explicit BenchScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { bench_.reset(); }
    void update() override;
    void draw() override;

  private:
    void finish_phase();

    const Demos& demos_;
    std::unique_ptr<PrefabsBench> bench_; // on the heap: three worlds, three libraries and a map
    int phase_ = 0;
    int frame_ = 0;
    nnh::Duration total_{}, worst_{}, hand_total_{};
    char failed_[64] = {};
    bool verdict_ = false;
    int sink_ = 0;
    char lines_[kPhases][128] = {};
};
