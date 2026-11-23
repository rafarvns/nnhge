#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <new>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>

namespace {

struct Phase {
    const char* name;
    double target_ms; ///< 0: information only
    int (EntitiesBench::*run)();
};

// AC-7's targets, as the spec wrote them before any console measurement.
constexpr Phase kPhaseList[BenchScene::kPhases] = {
    {"each", 0.5, &EntitiesBench::run_each},     {"spawn", 1.0, &EntitiesBench::run_spawns},
    {"find", 0.1, &EntitiesBench::run_find},     {"get", 0.5, &EntitiesBench::run_get},
    {"storage get", 0.0, &EntitiesBench::run_get_storage}, {"pool", 0.3, &EntitiesBench::run_pool},
};

double ms(nnh::Duration d) { return d.seconds() * 1000.0; }

// Unoptimised code says nothing about the console's numbers: a Debug build reports them without judging.
constexpr bool kJudged = nnh::build::flavor() != nnh::build::Flavor::Debug;

} // namespace

void BenchScene::on_enter() {
    bench_.reset(new (std::nothrow) EntitiesBench());
    if (bench_ && !bench_->start()) bench_.reset();
    phase_ = 0;
    frame_ = 0;
    total_ = worst_ = nnh::Duration{};
    failed_[0] = '\0';
    verdict_ = false;
    for (auto& line : lines_) line[0] = '\0';
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "entities bench: started (6 phases of 300 frames: about 30 s)");
}

void BenchScene::finish_phase() {
    const Phase& p = kPhaseList[phase_];
    const double avg = ms(total_) / kFrames, worst = ms(worst_);
    if (p.target_ms <= 0) {
        std::snprintf(lines_[phase_], sizeof lines_[phase_], "entities bench: %s avg %.3f ms worst %.3f ms (information)",
                      p.name, avg, worst);
    } else {
        const bool pass = !kJudged || avg < p.target_ms;
        std::snprintf(lines_[phase_], sizeof lines_[phase_],
                      "entities bench: %s avg %.3f ms worst %.3f ms (target < %.1f ms) %s", p.name, avg, worst,
                      p.target_ms, !kJudged ? "not judged (Debug)" : (pass ? "PASS" : "FAIL"));
        if (!pass) {
            const std::size_t at = std::strlen(failed_);
            std::snprintf(failed_ + at, sizeof failed_ - at, "%s%s", at == 0 ? "" : ", ", p.name);
        }
    }
    NNH_LOG_INFO(example, "%s", lines_[phase_]);
    ++phase_;
    frame_ = 0;
    total_ = worst_ = nnh::Duration{};
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::replace(*demos_.showcase);
        return;
    }
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
    if (!bench_) return;
    if (phase_ >= kPhases) {
        if (!verdict_) { // the frame after the last phase's line: each line can be expected on its own
            verdict_ = true;
            if (failed_[0] == '\0') {
                NNH_LOG_INFO(example, "entities bench: PASS");
            } else {
                NNH_LOG_INFO(example, "entities bench: FAIL (%s)", failed_);
            }
        }
        return;
    }
    const Phase& p = kPhaseList[phase_];
    const nnh::Duration start = nnh::time::now();
    {
        NNH_PROFILE_SCOPE("entities bench");
        sink_ += ((*bench_).*(p.run))();
    }
    const nnh::Duration took = nnh::time::now() - start;
    total_ = total_ + took;
    if (took > worst_) worst_ = took;
    if (++frame_ >= kFrames) finish_phase();
}

void BenchScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 30}, "ENTITIES BENCH", {.scale = 2, .align = nnh::TextAlign::Center});
    char text[64];
    if (phase_ < kPhases) {
        std::snprintf(text, sizeof text, "%s: frame %d of %d", kPhaseList[phase_].name, frame_, kFrames);
    } else {
        std::snprintf(text, sizeof text, "%s", failed_[0] == '\0' ? "PASS" : "FAIL");
    }
    gfx::draw_text(nnh::Screen::Top, {200, 80}, text, {.align = nnh::TextAlign::Center});
    for (int i = 0; i < kPhases; ++i)
        gfx::draw_text(nnh::Screen::Bottom, {4, static_cast<float>(8 + 14 * i)}, lines_[i] + 16,
                       {.color = nnh::rgb(200, 210, 230)});
    gfx::draw_text(nnh::Screen::Bottom, {4, 220}, "SELECT showcase  START quit", {.color = nnh::rgb(150, 160, 190)});
}
