#pragma once
// The pathfinding bench (spec 025 AC-7): bench.hpp's workloads, one call per frame for 300 frames each with the clock,
// one log line per workload: "pathfinding bench: <name> avg X <unit> worst Y (target < Z) PASS|FAIL" (Debug: "not
// judged (Debug)"; next_crowd is logged, never judged), then "pathfinding bench: PASS" or "pathfinding bench: FAIL
// (<names>)" in the next frame. SELECT goes back to the showcase.

#include <memory>
#include <nnh/scene.hpp>
#include <nnh/time.hpp>

#include "bench.hpp"
#include "demos.hpp"

class BenchScene final : public nnh::Scene {
public:
    static constexpr int kFrames = 300;
    static constexpr int kPhases = 11;

    explicit BenchScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { bench_.reset(); }
    void update() override;
    void draw() override;

private:
    void finish_phase();

    const Demos& demos_;
    std::unique_ptr<PathBench> bench_; // on the heap: maps, a 128 x 128 searcher, fields, a tilemap
    int phase_ = 0;
    int frame_ = 0;
    double total_ = 0;   ///< ms
    double worst_ = 0;   ///< ms (astar: us per expanded cell)
    long long units_ = 0; ///< astar: expansions
    char failed_[128] = {};
    bool verdict_ = false;
    int sink_ = 0;
    char lines_[kPhases][112] = {};
};
