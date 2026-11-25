#pragma once
// The models check (spec 026 AC-7): the 50 000-operation math script (self_check.hpp) in slices of 1 000 a frame, six
// fixed probe scenes whose pixels are read back (gfx3d::request_probes) and compared with FR-10's formula within
// FR-24's tolerance (026a's order and timing), then the stereo script (026b) and the stereo probe scene in both eyes
// when the slider is up. Logs "3d: <n> sum <x>", "3d: PASS <x>", "3d pixels: PASS" (or the first FAIL with its scene,
// pixel, got and want), "3d-stereo: <n> sum <x>", "3d-stereo: PASS <x>", "3d pixels stereo: PASS" (or "skipped (slider
// at 0)"). SELECT: back to the showcase; START: quit.

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
    enum class Stage { Math, Stereo, Pixels, StereoPixels, Done };

    /// Runs a script's slice; logs its checkpoints and (the frame after the last one) its verdict. @return true once
    /// the verdict is logged.
    bool run_script(models::SelfCheck& script, const char* tag, std::uint32_t golden);
    /// Phase 1 of a probe scene: compares the answer when it comes. @return true when this scene is finished.
    bool check_probes(const ProbeExpect* e, int n, const char* scene);
    void update_pixels();
    void update_stereo_pixels();

    World& world_;
    Demos& demos_;
    models::SelfCheck math_;
    models::SelfCheck stereo_{models::Script::Stereo};
    ProbeScenes probes_;
    Stage stage_ = Stage::Math;
    bool script_done_ = false; ///< the running script's last slice ran (its verdict follows next frame)
    int scene_ = 0;  ///< probe scene being shown (Pixels), or the eye being read (StereoPixels)
    int phase_ = 0;  ///< 0: drawing, 1: asked, waiting for the answer
    int waited_ = 0; ///< frames phase 1 has waited
    static constexpr int kMaxProbeWait = 10;
    bool pixels_failed_ = false;
    float slider_ = 0;
    char status_[96] = {};
};
