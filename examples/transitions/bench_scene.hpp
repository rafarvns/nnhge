#pragma once
// The bench (spec 023 AC-8): the base run, then each workload's 300 transition frames, measured from the previous
// frame's profile scopes in draw() (scenes don't update during a transition; they draw). Logs one line per workload
// and a verdict; Debug and Release builds are not judged (Development measures).

#include <nnh/scene.hpp>

#include "bench.hpp"

struct Demos;

class BenchScene final : public nnh::Scene {
public:
    explicit BenchScene(Demos& demos) : demos_(demos) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    void measure();
    void finish_workload();

    Demos& demos_;
    int load_ = 0;
    int frame_ = 0;
    int slices_ = 0;
    bool done_ = false;
    BenchStats stats_[static_cast<int>(BenchLoad::Count)]{};
    BenchStats probe_{}; ///< the empty `bench probe` scope: one profiler scope's own cost
    char shown_[512] = {};
};
