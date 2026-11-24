#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <new>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>

namespace {

enum class Judge { Avg, Worst, None };

struct Phase {
    const char* name;
    const char* unit;
    double target;
    Judge judge;
    int (PathBench::*run)();
};

// AC-7's budgets, set from the Old 3DS measurement after the optimisation round (2026-11-24, console run 2: astar
// 4.049 us, maze one 0.714, maze slices worst 0.497, replans 14.606, field one 6.613, field slices worst 1.366, next
// 0.165, smooth 0.225, sync 3.840, door 0.038 ms) with ~25 % margin.
constexpr Phase kPhaseList[BenchScene::kPhases] = {
    {"astar", "us", 5.0, Judge::Avg, &PathBench::astar}, // per expanded cell
    {"maze one", "ms", 0.9, Judge::Avg, &PathBench::maze_one},
    {"maze slices", "ms", 0.65, Judge::Worst, &PathBench::maze_slice},
    {"replans", "ms", 18.0, Judge::Avg, &PathBench::replans},
    {"field one", "ms", 8.3, Judge::Avg, &PathBench::field_one},
    {"field slices", "ms", 1.7, Judge::Worst, &PathBench::field_slice},
    {"next", "ms", 0.21, Judge::Avg, &PathBench::next},
    {"next_crowd", "ms", 0, Judge::None, &PathBench::next_crowd},
    {"smooth", "ms", 0.3, Judge::Avg, &PathBench::smooth},
    {"sync", "ms", 4.8, Judge::Avg, &PathBench::sync},
    {"door", "ms", 0.05, Judge::Avg, &PathBench::door},
};

// Unoptimised code says nothing about the console's numbers: a Debug build reports them without judging.
constexpr bool kJudged = nnh::build::flavor() != nnh::build::Flavor::Debug;

} // namespace

void BenchScene::on_enter() {
    bench_.reset(new (std::nothrow) PathBench());
    if (bench_ && !bench_->init()) bench_.reset();
    phase_ = frame_ = 0;
    total_ = worst_ = 0;
    units_ = 0;
    failed_[0] = '\0';
    verdict_ = false;
    for (auto& line : lines_) line[0] = '\0';
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "pathfinding bench: started (11 workloads of 300 frames: about 55 s)");
}

void BenchScene::finish_phase() {
    const Phase& p = kPhaseList[phase_];
    const bool per_cell = phase_ == 0;
    const double avg = per_cell ? total_ * 1000.0 / static_cast<double>(units_ > 0 ? units_ : 1) : total_ / kFrames;
    const double judged = p.judge == Judge::Worst ? worst_ : avg;
    const bool pass = !kJudged || p.judge == Judge::None || judged < p.target;
    const char* verdict = p.judge == Judge::None ? "not judged" : (!kJudged ? "not judged (Debug)" : (pass ? "PASS" : "FAIL"));
    if (p.judge == Judge::None) {
        std::snprintf(lines_[phase_], sizeof lines_[phase_], "pathfinding bench: %s avg %.3f %s worst %.3f %s",
                      p.name, avg, p.unit, worst_, verdict);
    } else {
        std::snprintf(lines_[phase_], sizeof lines_[phase_],
                      "pathfinding bench: %s avg %.3f %s worst %.3f (target < %g %s%s) %s", p.name, avg, p.unit,
                      worst_, p.target, p.unit, p.judge == Judge::Worst ? ", worst" : "", verdict);
    }
    if (!pass) {
        const std::size_t at = std::strlen(failed_);
        std::snprintf(failed_ + at, sizeof failed_ - at, "%s%s", at == 0 ? "" : ", ", p.name);
    }
    NNH_LOG_INFO(example, "%s", lines_[phase_]);
    ++phase_;
    frame_ = 0;
    total_ = worst_ = 0;
    units_ = 0;
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::replace(*demos_.showcase);
        return;
    }
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
    if (!bench_) return;
    if (phase_ >= kPhases) {
        if (!verdict_) { // the frame after the last line: each line can be expected on its own
            verdict_ = true;
            if (failed_[0] == '\0') {
                NNH_LOG_INFO(example, "pathfinding bench: PASS");
            } else {
                NNH_LOG_INFO(example, "pathfinding bench: FAIL (%s)", failed_);
            }
        }
        return;
    }
    if (std::strcmp(kPhaseList[phase_].name, "door") == 0) bench_->prepare_door(); // untimed
    const nnh::Duration start = nnh::time::now();
    {
        NNH_PROFILE_SCOPE("pathfinding bench");
        sink_ += ((*bench_).*(kPhaseList[phase_].run))();
    }
    const double ms = (nnh::time::now() - start).seconds() * 1000.0;
    total_ += ms;
    if (phase_ == 0) {
        units_ += bench_->last_expanded();
        const double per = ms * 1000.0 / static_cast<double>(bench_->last_expanded() > 0 ? bench_->last_expanded() : 1);
        if (per > worst_) worst_ = per;
    } else if (ms > worst_) {
        worst_ = ms;
    }
    if (++frame_ >= kFrames) finish_phase();
}

void BenchScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 30}, "PATHFINDING BENCH", {.scale = 2, .align = nnh::TextAlign::Center});
    char text[64];
    if (!bench_) {
        std::snprintf(text, sizeof text, "no memory");
    } else if (phase_ < kPhases) {
        std::snprintf(text, sizeof text, "%s: frame %d of %d", kPhaseList[phase_].name, frame_, kFrames);
    } else {
        std::snprintf(text, sizeof text, "%s", failed_[0] == '\0' ? "PASS" : "FAIL");
    }
    gfx::draw_text(nnh::Screen::Top, {200, 80}, text, {.align = nnh::TextAlign::Center});
    for (int i = 0; i < kPhases; ++i) {
        gfx::draw_text(nnh::Screen::Bottom, {2, static_cast<float>(4 + 18 * i)}, lines_[i] + 19,
                       {.color = nnh::rgb(200, 210, 230)});
    }
    gfx::draw_text(nnh::Screen::Bottom, {4, 224}, "SELECT showcase  START quit", {.color = nnh::rgb(150, 160, 190)});
}
