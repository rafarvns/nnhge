#pragma once
// The saves example's bench (spec 027 AC-8): the memory workloads 100 times each (10 per frame, averaged),
// the card workloads 10 times each (one per frame: median, and the store's worst), each line logged as "saves
// bench: <name> ... (target ...) PASS|FAIL", then "saves bench: PASS" or "saves bench: FAIL (<names>)". A
// Debug build says "not judged (Debug)". SELECT goes back to the showcase; START quits.

#include "bench.hpp"
#include "demos.hpp"

#include <memory>
#include <nnh/time.hpp>

class BenchScene final : public nnh::Scene {
  public:
    static constexpr int kPhases = 8;

    explicit BenchScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

  private:
    void run_frame();
    void finish_phase();

    const Demos& demos_;
    std::unique_ptr<saves_example::SavesBench> bench_; ///< made on enter, on the heap (≈ 25 KB)
    int phase_ = 0;
    int runs_ = 0;
    bool prepared_ = false;
    bool verdict_ = false;
    bool failed_run_ = false;
    double samples_[100] = {};
    char failed_[96] = {};
    char lines_[kPhases][112] = {};
};
