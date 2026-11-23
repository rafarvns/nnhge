#pragma once
// The entities bench (spec 020 AC-7): each workload of bench.hpp timed over 300 frames with the clock, one phase after
// the other, one log line per phase: "entities bench: <name> avg X ms worst Y ms (target < Z ms) PASS|FAIL" (the
// storage lookups: "(information)"; a Debug build: "not judged (Debug)"), then the verdict "entities bench: PASS" or
// "entities bench: FAIL (<names>)" in the next frame (check.nnhs fails on it, so one console run reports every number). SELECT goes back to the showcase.

#include "bench.hpp"
#include "demos.hpp"

#include <memory>
#include <nnh/scene.hpp>
#include <nnh/time.hpp>

class BenchScene final : public nnh::Scene {
  public:
    static constexpr int kFrames = 300;
    static constexpr int kPhases = 6;

    explicit BenchScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { bench_.reset(); }
    void update() override;
    void draw() override;

  private:
    void finish_phase();

    const Demos& demos_;
    std::unique_ptr<EntitiesBench> bench_; // on the heap: worlds and a 1024-slot pool
    int phase_ = 0;
    int frame_ = 0;
    nnh::Duration total_{}, worst_{};
    char failed_[64] = {};
    bool verdict_ = false;
    int sink_ = 0;
    char lines_[kPhases][96] = {};
};
