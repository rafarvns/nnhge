#pragma once
// The models bench (spec 026 AC-8): four workloads of 600 frames each, mono — throughput (one textured, lit sphere of
// 12 032 triangles filling about half the screen), draws (256 cubes, two materials, one directional and two point
// lights), cpu (512 draws, half outside the view, 128 of them blended) and fill (three full-screen blended planes over
// an opaque scene) — plus the HUD. Logs "3d bench: <case> avg <ms> worst <ms> missed <n>/600 rec <ms> sub <ms> prep <ms>
// cpu <ms>" per case (per frame: rec = the game's recording, sub = the "gfx submit" scope, prep = the "gfx3d prepare"
// scope (culling and ordering, inside sub), cpu = AC-8's CPU share: rec + sub for draws, rec + prep for cpu; sub and
// prep exist in Development builds only) and "3d bench: PASS" or "3d bench: FAIL ..." against AC-8's targets.
// Then (spec 026b, AC-8 stereo) two stereo cases with the slider up — "throughput 6000 stereo" (a 6 016-triangle sphere)
// and "draws 256 stereo" — logged the same way and "not judged" until their budgets come from the console: after the
// mono cases the bench logs "3d bench: stereo waiting for the slider" and waits up to 300 frames for it to rise (a
// script's `slider 1`), else "3d bench: stereo skipped (slider at 0)". Mono cases turn stereo off on their camera.
// MODELS_BENCH_FRAMES shortens the cases (simulator smoke runs).

#include <nnh/scene.hpp>

#include "world.hpp"

class BenchScene : public nnh::Scene {
public:
    BenchScene(World& world, Demos& demos) : world_(world), demos_(demos) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    void finish_case();
    /// Before the stereo cases: true once the slider is up (or they are skipped).
    bool wait_for_slider();

    World& world_;
    Demos& demos_;
    nnh::Mesh dense_;   ///< the throughput sphere (48 rings × 128 segments)
    nnh::Mesh half_;    ///< the stereo throughput sphere (48 rings × 64 segments)
    int slider_wait_ = -1; ///< frames waited for the slider (−1: not waiting yet)
    nnh::Mesh plane_;   ///< a 1×1 plane for the fill case
    int frames_per_case_ = 600;
    int case_ = 0;
    int frame_ = 0;
    std::int64_t sum_ns_ = 0, worst_ns_ = 0, rec_ns_ = 0, sub_ns_ = 0, prep_ns_ = 0;
    int missed_ = 0;
    bool failed_ = false;
    bool verdict_logged_ = false;
    bool done_ = false;
    char line_[128] = {};
};
