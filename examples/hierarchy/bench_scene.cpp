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
    double target_ms;
    int (HierarchyBench::*run)();
};

// AC-7's budgets (spec 022): the Old 3DS measurement after the optimisation round (plan 022 "Console run 2 and budgets
// set") plus ~25%; pass_move keeps its 1 ms target. spawn: the hand-written calls take 2.9 ms themselves.
constexpr Phase kPhaseList[BenchScene::kPhases] = {
    {"pass_turn", 1.45, &HierarchyBench::run_pass},
    {"pass_move", 1.0, &HierarchyBench::run_pass},
    {"world_now", 1.4, &HierarchyBench::run_world_now},
    {"spawn", 3.6, &HierarchyBench::run_spawn},
    {"reparent", 1.15, &HierarchyBench::run_reparent},
};
constexpr int kSpawnPhase = 3;
constexpr double kMaxRatio = 1.3; ///< prefab hierarchies vs the hand-written calls, measured in the same run

double ms(nnh::Duration d) { return d.seconds() * 1000.0; }

// Unoptimised code says nothing about the console's numbers: a Debug build reports them without judging.
constexpr bool kJudged = nnh::build::flavor() != nnh::build::Flavor::Debug;

} // namespace

void BenchScene::on_enter() {
    bench_.reset(new (std::nothrow) HierarchyBench());
    if (bench_ && !bench_->start()) bench_.reset();
    phase_ = 0;
    frame_ = 0;
    total_ = worst_ = hand_total_ = nnh::Duration{};
    failed_[0] = '\0';
    verdict_ = false;
    for (auto& line : lines_) line[0] = '\0';
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "hierarchy bench: started (5 phases of 300 frames: about 25 s)");
}

void BenchScene::finish_phase() {
    const Phase& p = kPhaseList[phase_];
    const bool spawn = phase_ == kSpawnPhase;
    const double frames = spawn ? kFrames / 2 : kFrames; // spawn: prefab and hand-written frames alternate
    const double avg = ms(total_) / frames, worst = ms(worst_);
    const double hand = spawn ? ms(hand_total_) / frames : 0, ratio = hand > 0 ? avg / hand : 0;
    const bool pass = !kJudged || (avg < p.target_ms && (!spawn || ratio <= kMaxRatio));
    const char* verdict = !kJudged ? "not judged (Debug)" : (pass ? "PASS" : "FAIL");
    if (spawn) {
        std::snprintf(lines_[phase_], sizeof lines_[phase_],
                      "hierarchy bench: %s avg %.3f ms worst %.3f ms hand %.3f ms ratio %.2f (target < %g ms, <= %.2f) "
                      "%s",
                      p.name, avg, worst, hand, ratio, p.target_ms, kMaxRatio, verdict);
    } else {
        std::snprintf(lines_[phase_], sizeof lines_[phase_],
                      "hierarchy bench: %s avg %.3f ms worst %.3f ms (target < %g ms) %s", p.name, avg, worst,
                      p.target_ms, verdict);
    }
    if (!pass) {
        const std::size_t at = std::strlen(failed_);
        std::snprintf(failed_ + at, sizeof failed_ - at, "%s%s", at == 0 ? "" : ", ", p.name);
    }
    NNH_LOG_INFO(example, "%s", lines_[phase_]);
    ++phase_;
    frame_ = 0;
    total_ = worst_ = hand_total_ = nnh::Duration{};
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
                NNH_LOG_INFO(example, "hierarchy bench: PASS");
            } else {
                NNH_LOG_INFO(example, "hierarchy bench: FAIL (%s)", failed_);
            }
        }
        return;
    }
    const bool hand = phase_ == kSpawnPhase && frame_ % 2 == 1; // alternate frames: the same cache conditions
    if (phase_ == 0) bench_->prepare_turn();                     // untimed: the moves before the pass
    if (phase_ == 1) bench_->prepare_move();
    const nnh::Duration start = nnh::time::now();
    {
        NNH_PROFILE_SCOPE("hierarchy bench");
        sink_ += hand ? bench_->run_hand() : ((*bench_).*(kPhaseList[phase_].run))();
    }
    const nnh::Duration took = nnh::time::now() - start;
    if (hand) {
        hand_total_ = hand_total_ + took;
    } else {
        total_ = total_ + took;
        if (took > worst_) worst_ = took;
    }
    if (++frame_ >= kFrames) finish_phase();
}

void BenchScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 30}, "HIERARCHY BENCH", {.scale = 2, .align = nnh::TextAlign::Center});
    char text[64];
    if (phase_ < kPhases) {
        std::snprintf(text, sizeof text, "%s: frame %d of %d", kPhaseList[phase_].name, frame_, kFrames);
    } else {
        std::snprintf(text, sizeof text, "%s", failed_[0] == '\0' ? "PASS" : "FAIL");
    }
    gfx::draw_text(nnh::Screen::Top, {200, 80}, text, {.align = nnh::TextAlign::Center});
    for (int i = 0; i < kPhases; ++i)
        gfx::draw_text(nnh::Screen::Bottom, {4, static_cast<float>(8 + 24 * i)}, lines_[i] + 17,
                       {.color = nnh::rgb(200, 210, 230)});
    gfx::draw_text(nnh::Screen::Bottom, {4, 220}, "SELECT showcase  START quit", {.color = nnh::rgb(150, 160, 190)});
}
