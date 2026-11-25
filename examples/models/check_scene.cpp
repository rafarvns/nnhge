#include "check_scene.hpp"

#include <cstdio>
#include <cstdlib>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/system.hpp>

namespace {

constexpr int kOpsPerFrame = 1000;

bool close(nnh::Color got, nnh::Color want, int tol) {
    return std::abs(got.r - want.r) <= tol && std::abs(got.g - want.g) <= tol && std::abs(got.b - want.b) <= tol;
}

} // namespace

void CheckScene::on_enter() {
    math_ = models::SelfCheck{};
    stereo_ = models::SelfCheck{models::Script::Stereo};
    stage_ = Stage::Math;
    script_done_ = pixels_failed_ = false;
    scene_ = phase_ = waited_ = 0;
    std::snprintf(status_, sizeof status_, "running the math script");
    NNH_LOG_INFO(example, "models: check started");
    if (!probes_.load(world_)) NNH_LOG_ERROR(example, "3d pixels: FAIL the probe scenes could not be made");
}

bool CheckScene::run_script(models::SelfCheck& script, const char* tag, std::uint32_t golden) {
    if (script_done_) { // the frame after the last checkpoint: each line expectable on its own
        script_done_ = false;
        const bool pass = script.sum() == golden;
        NNH_LOG_INFO(example, "%s: %s %08X", tag, pass ? "PASS" : "FAIL", static_cast<unsigned>(script.sum()));
        std::snprintf(status_, sizeof status_, "%s: %s %08X", tag, pass ? "PASS" : "FAIL",
                      static_cast<unsigned>(script.sum()));
        return true;
    }
    const int before = script.done();
    script_done_ = script.run(kOpsPerFrame);
    const int every = models::SelfCheck::kCheckpointEvery;
    if (script.done() / every != before / every) {
        const int k = script.done() / every;
        NNH_LOG_INFO(example, "%s: %d sum %08X", tag, k * every, static_cast<unsigned>(script.checkpoint(k)));
    }
    return false;
}

bool CheckScene::check_probes(const ProbeExpect* e, int n, const char* scene) {
    nnh::Color got[ProbeScenes::kMaxProbes] = {};
    const int answered = nnh::gfx3d::probe_results(got, n);
    if (answered < n && ++waited_ < kMaxProbeWait) return false; // the same scene is drawn meanwhile
    for (int i = 0; i < n && !pixels_failed_; ++i) {
        if (answered == n && close(got[i], e[i].want, e[i].tolerance)) continue;
        pixels_failed_ = true;
        if (answered < n) {
            NNH_LOG_INFO(example, "3d pixels: FAIL %s: no answer after %d frames", scene, waited_);
            break;
        }
        NNH_LOG_INFO(example, "3d pixels: FAIL %s %d,%d got %d,%d,%d want %d,%d,%d", scene, e[i].at.x, e[i].at.y,
                     got[i].r, got[i].g, got[i].b, e[i].want.r, e[i].want.g, e[i].want.b);
    }
    phase_ = 0;
    waited_ = 0;
    return true;
}

void CheckScene::update_pixels() {
    if (phase_ == 0) {
        phase_ = 1; // asked in draw() (gfx3d.hpp request_probes); the answer comes a few frames later
        return;
    }
    ProbeExpect e[ProbeScenes::kMaxProbes];
    const int n = probes_.expect(scene_, e);
    if (!check_probes(e, n, ProbeScenes::name(scene_))) return;
    if (!pixels_failed_ && ++scene_ < ProbeScenes::kScenes) return;
    if (!pixels_failed_) NNH_LOG_INFO(example, "3d pixels: PASS");
    std::snprintf(status_, sizeof status_, "3d pixels: %s", pixels_failed_ ? "FAIL" : "PASS");
    stage_ = pixels_failed_ ? Stage::Done : Stage::Stereo;
    scene_ = 0;
}

void CheckScene::update_stereo_pixels() {
    if (phase_ == 0 && scene_ == 0 && waited_ == 0) {
        slider_ = nnh::system::stereo_slider();
        if (slider_ <= 0.0f) { // the console or the script keeps the slider down, or a 2DS
            NNH_LOG_INFO(example, "3d pixels stereo: skipped (slider at 0)");
            stage_ = Stage::Done;
            return;
        }
    }
    if (phase_ == 0) {
        phase_ = 1;
        return;
    }
    ProbeExpect e[ProbeScenes::kMaxProbes];
    const int n = ProbeScenes::expect_stereo(scene_, e);
    if (!check_probes(e, n, scene_ == 0 ? "stereo-left" : "stereo-right")) return;
    if (!pixels_failed_ && ++scene_ < 2) return;
    if (!pixels_failed_) NNH_LOG_INFO(example, "3d pixels stereo: PASS");
    std::snprintf(status_, sizeof status_, "3d pixels stereo: %s", pixels_failed_ ? "FAIL" : "PASS");
    stage_ = Stage::Done;
}

void CheckScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) return nnh::scenes::pop();
    if (nnh::input::pressed(nnh::Button::Select) && demos_.showcase != nullptr)
        return nnh::scenes::replace(*demos_.showcase);
    switch (stage_) {
    case Stage::Math: // then the mono pixels (026a's order and timing), the stereo script, the stereo pixels
        if (!run_script(math_, "3d", models::kSelfCheckGolden)) return;
        stage_ = Stage::Pixels;
        if (!nnh::build::kDevToolsEnabled) {
            NNH_LOG_INFO(example, "3d pixels: skipped (Release reads no pixels back)");
            stage_ = Stage::Stereo;
        }
        return;
    case Stage::Stereo:
        if (!run_script(stereo_, "3d-stereo", models::kStereoGolden)) return;
        stage_ = nnh::build::kDevToolsEnabled ? Stage::StereoPixels : Stage::Done;
        return;
    case Stage::Pixels: return update_pixels();
    case Stage::StereoPixels: return update_stereo_pixels();
    case Stage::Done: return;
    }
}

void CheckScene::draw() {
    const bool failed_here = stage_ == Stage::Done && pixels_failed_; // a failed scene stays on screen (screenshot)
    if (stage_ == Stage::Pixels || (failed_here && scene_ < ProbeScenes::kScenes && slider_ == 0.0f)) {
        probes_.draw(scene_ < ProbeScenes::kScenes ? scene_ : 0);
    } else if (stage_ == Stage::StereoPixels || (failed_here && slider_ > 0.0f)) {
        probes_.draw_stereo(slider_ > 0.0f ? slider_ : 1.0f);
    }
    if (phase_ == 1 && waited_ == 0 && (stage_ == Stage::Pixels || stage_ == Stage::StereoPixels)) {
        ProbeExpect e[ProbeScenes::kMaxProbes];
        nnh::Probe at[ProbeScenes::kMaxProbes];
        const bool stereo = stage_ == Stage::StereoPixels;
        const int n = stereo ? ProbeScenes::expect_stereo(scene_, e) : probes_.expect(scene_, e);
        for (int i = 0; i < n; ++i) at[i] = e[i].at;
        nnh::gfx3d::request_probes(nnh::Screen::Top, stereo && scene_ == 1 ? nnh::Eye::Right : nnh::Eye::Left, at, n);
    }
    const models::SelfCheck& script = stage_ == Stage::Math || stage_ == Stage::Pixels ? math_ : stereo_;
    char text[192]; // fixed text, two numbers and status_ (96): never truncated (GCC 14 -Wformat-truncation)
    std::snprintf(text, sizeof text, "models check\n%d / %d operations\n%s\n\nSELECT showcase  START quit",
                  script.done(), models::SelfCheck::kOps, status_);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 8}, text, {});
}
