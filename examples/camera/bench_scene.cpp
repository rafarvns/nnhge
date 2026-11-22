#include "bench_scene.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace {

constexpr double kTargetMs = 0.05;

nnh::CameraSettings bench_settings(nnh::CameraLayout layout, nnh::Screen screen) {
    return {.screen = screen,
            .layout = layout,
            .dead_zone = nnh::CameraSettings::centered_zone(48, 32),
            .smoothing = {0.2f, 0.1f},
            .limits = {0, 0, 1280, 768}};
}

} // namespace

void BenchScene::on_enter() {
    tall_ = nnh::CameraRig(bench_settings(nnh::CameraLayout::TallWorld, nnh::Screen::Top));
    bottom_ = nnh::CameraRig(bench_settings(nnh::CameraLayout::Single, nnh::Screen::Bottom));
    tall_.set_limits({0, 0, 1280, 768});
    bottom_.set_limits({0, 0, 1280, 768});
    frames_ = 0;
    total_ = worst_ = nnh::Duration{};
    result_[0] = '\0';
    NNH_LOG_INFO(example, "camera: bench started, %d pairs per frame for %d frames", kPairsPerFrame, kFrames);
}

void BenchScene::update() {
    const float f = static_cast<float>(frames_);
    const nnh::Duration start = nnh::time::now();
    for (int i = 0; i < kPairsPerFrame; ++i) {
        // Trauma every so often so the shake path runs too.
        if ((frames_ + i) % 45 == 0) {
            tall_.add_trauma(0.5f);
            bottom_.add_trauma(0.5f);
        }
        const nnh::Vec2 target{200 + f * 1.5f + static_cast<float>(i),
                               300 + static_cast<float>((frames_ * 7) % 200)};
        tall_.update(target);
        tall_.apply();
        bottom_.update(target);
        bottom_.apply();
    }
    const nnh::Duration spent = nnh::time::now() - start;
    if (frames_ < kFrames) {
        total_ += spent;
        if (spent > worst_) worst_ = spent;
        if (++frames_ == kFrames) report();
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.play);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void BenchScene::report() {
    const double avg = total_.seconds() * 1000.0 / kFrames / kPairsPerFrame;
    const double worst = worst_.seconds() * 1000.0 / kPairsPerFrame;
    std::snprintf(result_, sizeof result_,
                  "camera: bench avg %.4f ms worst %.4f ms per pair (target < %.2f ms) %s", avg, worst,
                  kTargetMs, avg < kTargetMs ? "PASS" : "FAIL");
    NNH_LOG_INFO(example, "%s", result_);
    std::snprintf(shown_, sizeof shown_,
                  "per pair of rigs:\navg   %.4f ms\nworst %.4f ms\ntarget < %.2f ms: %s", avg, worst,
                  kTargetMs, avg < kTargetMs ? "PASS" : "FAIL");
}

void BenchScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::set_camera(nnh::Screen::Top, {});
    gfx::set_camera(nnh::Screen::Bottom, {});
    gfx::draw_text(nnh::Screen::Top, {200, 40}, "CAMERA BENCH",
                   {.scale = 2, .align = nnh::TextAlign::Center});
    char info[64];
    std::snprintf(info, sizeof info, "frame %d/%d, %d pairs per frame", frames_, kFrames, kPairsPerFrame);
    gfx::draw_text(nnh::Screen::Top, {200, 90}, info, {.align = nnh::TextAlign::Center});
    gfx::draw_text(nnh::Screen::Bottom, {8, 8}, result_[0] != '\0' ? shown_ : "measuring...",
                   {.color = nnh::colors::yellow});
}
