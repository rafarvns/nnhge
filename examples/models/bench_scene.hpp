#pragma once
// The models bench (spec 026 AC-8): four workloads of 600 frames each, mono — throughput (one textured, lit sphere of
// 12 032 triangles filling about half the screen), draws (256 cubes, two materials, one directional and two point
// lights), cpu (512 draws, half outside the view, 128 of them blended) and fill (three full-screen blended planes over
// an opaque scene) — plus the HUD. Logs "3d bench: <case> avg <ms> worst <ms> missed <n>/600 rec <ms> sub <ms> prep <ms>
// cpu <ms>" per case (per frame: rec = the game's recording, sub = the "gfx submit" scope, prep = the "gfx3d prepare"
// scope (culling and ordering, inside sub), cpu = AC-8's CPU share: rec + sub for draws, rec + prep for cpu; sub and
// prep exist in Development builds only) and "3d bench: PASS" or "3d bench: FAIL ..." against AC-8's targets.
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

    World& world_;
    Demos& demos_;
    nnh::Mesh dense_;   ///< the throughput sphere (48 rings × 128 segments)
    nnh::Mesh plane_;   ///< a 1×1 plane for the fill case
    int frames_per_case_ = 600;
    int case_ = 0;
    int frame_ = 0;
    std::int64_t sum_ns_ = 0, worst_ns_ = 0, rec_ns_ = 0, sub_ns_ = 0, prep_ns_ = 0;
    int missed_ = 0;
    bool failed_ = false;
    bool done_ = false;
    char line_[128] = {};
};
