#pragma once
// The models check (spec 026 AC-7): the 50 000-operation math script (self_check.hpp) in slices of 1 000 a frame, then
// six fixed probe scenes whose pixels are read back (gfx3d::request_probes) and compared with FR-10's formula within
// FR-24's tolerance. Logs "3d: <n> sum <x>", "3d: PASS <x>", then "3d pixels: PASS" (or the first FAIL with its scene,
// pixel, got and want). SELECT: back to the showcase; START: quit.

#include <nnh/mesh.hpp>
#include <nnh/scene.hpp>

#include "probe_scenes.hpp"
#include "self_check.hpp"
#include "world.hpp"

class CheckScene : public nnh::Scene {
public:
    CheckScene(World& world, Demos& demos) : world_(world), demos_(demos) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    World& world_;
    Demos& demos_;
    models::SelfCheck math_;
    ProbeScenes probes_;
    bool math_done_ = false;
    bool verdict_logged_ = false;
    int scene_ = 0;  ///< probe scene being shown
    int phase_ = 0;  ///< 0: drawing, 1: asked, waiting for the answer
    int waited_ = 0; ///< frames phase 1 has waited
    static constexpr int kMaxProbeWait = 10;
    bool pixels_done_ = false;
    bool pixels_failed_ = false;
    char status_[96] = {};
};
