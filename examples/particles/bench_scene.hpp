#pragma once
// The bench (spec 024 AC-7): bench.hpp's workloads, 300 frames each (the 60 Hz frame test 600), timed by the scene's
// clock around the measured calls plus, for drawing, spec 023's `gfx submit` profile scope (Development builds) minus
// the base run's. Logs `particles bench: <name> avg X ms worst Y ms (target < Z ms) PASS|FAIL`, the draw ratio line, the
// frame line, the off-screen line, then `particles bench: PASS` or `particles bench: FAIL (<names>)`. A Debug build
// logs every number "not judged (Debug)".

#include <memory>
#include <nnh/scene.hpp>
#include <nnh/texture.hpp>

#include "bench.hpp"
#include "demos.hpp"

class BenchScene final : public nnh::Scene {
public:
    static constexpr int kFrames = 300;
    static constexpr int kFrameTestFrames = 600;

    BenchScene(const Demos& demos, const nnh::Texture& sheet) : demos_(demos), sheet_(sheet) {}

    void on_enter() override;
    void on_exit() override { bench_.reset(); }
    void update() override;
    void draw() override;

    enum Phase { kBase, kUpdate1024, kUpdate16x64, kEmit512, kDraw512, kSprite512, kOffscreen, kFrame, kPhases };

private:
    struct Sum {
        double own = 0, own_worst = 0, submit = 0;
        int n = 0;
    };
    void finish();
    void verdict();
    double cost(Phase p) const; ///< own + submit − base submit, ms per frame

    const Demos& demos_;
    const nnh::Texture& sheet_;
    std::unique_ptr<ParticleBench> bench_; // ~650 KB of emitters: on the heap
    Sum sums_[kPhases];
    int phase_ = 0;
    int frame_ = 0;
    int missed_ = 0;
    double worst_frame_ = 0;
    int culled_ = 0;
    char failed_[96] = {};
    bool done_ = false;
};
