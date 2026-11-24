#include "check_scene.hpp"

#include <cstdio>
#include <cstdlib>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace {

constexpr int kOpsPerFrame = 1000;

bool close(nnh::Color got, nnh::Color want, int tol) {
    return std::abs(got.r - want.r) <= tol && std::abs(got.g - want.g) <= tol && std::abs(got.b - want.b) <= tol;
}

} // namespace

void CheckScene::on_enter() {
    math_ = models::SelfCheck{};
    math_done_ = verdict_logged_ = pixels_done_ = pixels_failed_ = false;
    scene_ = phase_ = 0;
    std::snprintf(status_, sizeof status_, "running the math script");
    NNH_LOG_INFO(example, "models: check started");
    if (!probes_.load(world_)) NNH_LOG_ERROR(example, "3d pixels: FAIL the probe scenes could not be made");
}

void CheckScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) return nnh::scenes::pop();
    if (nnh::input::pressed(nnh::Button::Select) && demos_.showcase != nullptr)
        return nnh::scenes::replace(*demos_.showcase);
    if (!math_done_) {
        const int before = math_.done();
        math_done_ = math_.run(kOpsPerFrame);
        if (math_.done() / models::SelfCheck::kCheckpointEvery != before / models::SelfCheck::kCheckpointEvery) {
            const int k = math_.done() / models::SelfCheck::kCheckpointEvery;
            NNH_LOG_INFO(example, "3d: %d sum %08X", k * models::SelfCheck::kCheckpointEvery,
                         static_cast<unsigned>(math_.checkpoint(k)));
        }
        return;
    }
    if (!verdict_logged_) { // the frame after the last checkpoint: each line expectable on its own
        verdict_logged_ = true;
        {
            const bool pass = math_.sum() == models::kSelfCheckGolden;
            NNH_LOG_INFO(example, "3d: %s %08X", pass ? "PASS" : "FAIL", static_cast<unsigned>(math_.sum()));
            std::snprintf(status_, sizeof status_, "3d: %s %08X", pass ? "PASS" : "FAIL",
                          static_cast<unsigned>(math_.sum()));
            if (!nnh::build::kDevToolsEnabled) {
                NNH_LOG_INFO(example, "3d pixels: skipped (Release reads no pixels back)");
                pixels_done_ = true;
            }
        }
        return;
    }
    if (pixels_done_) return;
    if (phase_ == 1) { // the request of the previous frame has its answer now
        ProbeExpect e[ProbeScenes::kMaxProbes];
        nnh::Color got[ProbeScenes::kMaxProbes];
        const int n = probes_.expect(scene_, e);
        const int answered = nnh::gfx3d::probe_results(got, n);
        for (int i = 0; i < n && !pixels_failed_; ++i) {
            if (answered == n && close(got[i], e[i].want, e[i].tolerance)) continue;
            pixels_failed_ = true;
            NNH_LOG_INFO(example, "3d pixels: FAIL %s %d,%d got %d,%d,%d want %d,%d,%d", ProbeScenes::name(scene_),
                         e[i].at.x, e[i].at.y, got[i].r, got[i].g, got[i].b, e[i].want.r, e[i].want.g, e[i].want.b);
        }
        phase_ = 0;
        if (pixels_failed_ || ++scene_ == ProbeScenes::kScenes) {
            pixels_done_ = true;
            if (!pixels_failed_) NNH_LOG_INFO(example, "3d pixels: PASS");
            std::snprintf(status_, sizeof status_, "3d pixels: %s", pixels_failed_ ? "FAIL" : "PASS");
        }
        return;
    }
    phase_ = 1;
}

void CheckScene::draw() {
    if (math_done_ && !pixels_done_) {
        probes_.draw(scene_);
        if (phase_ == 1) { // this frame is the one the request reads
            ProbeExpect e[ProbeScenes::kMaxProbes];
            nnh::Probe at[ProbeScenes::kMaxProbes];
            const int n = probes_.expect(scene_, e);
            for (int i = 0; i < n; ++i) at[i] = e[i].at;
            nnh::gfx3d::request_probes(nnh::Screen::Top, nnh::Eye::Left, at, n);
        }
    }
    char text[128];
    std::snprintf(text, sizeof text, "models check\n%d / %d operations\n%s\n\nSELECT showcase  START quit", math_.done(),
                  models::SelfCheck::kOps, status_);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 8}, text, {});
}
