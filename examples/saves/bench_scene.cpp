// The saves example's bench (bench_scene.hpp).

#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <new>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>

using saves_example::SavesBench;

namespace {

struct Phase {
    const char* name;
    bool card; ///< on the SD card: 10 runs, one per frame, the median; else 100 runs, 10 per frame, the
               ///< average
    double target_ms; ///< 0: no target (logged for comparison)
    double max_ms;    ///< the worst run's limit (store only)
    bool (SavesBench::*run)();
    bool (SavesBench::*prepare)();
};

// AC-8's budgets (spec 027): console run 2 on the Old 3DS XL (2026-11-25, after the T10.3 optimisation round) + ≈ 25 %
// (plan 027, "Console run 2 and budgets set"); the store's 300 ms worst case is the spec's own limit.
constexpr Phase kPhaseList[BenchScene::kPhases] = {
    {"encode", false, 0.25, 0, &SavesBench::encode, nullptr},
    {"decode", false, 0.48, 0, &SavesBench::decode, nullptr},
    {"decode v1", false, 0.29, 0, &SavesBench::decode_v1, nullptr},
    {"store", true, 165, 300, &SavesBench::store, nullptr},
    {"load", true, 45, 0, &SavesBench::load, nullptr},
    {"backup", true, 90, 0, &SavesBench::load_backup, &SavesBench::prepare_backup},
    {"summaries", true, 135, 0, &SavesBench::summaries, &SavesBench::prepare_summaries},
    {"raw write", true, 0, 0, &SavesBench::raw_write, nullptr},
};
constexpr int kMemoryRuns = 100;
constexpr int kMemoryPerFrame = 10;
constexpr int kCardRuns = 10;

// Unoptimised code says nothing about the console's numbers: a Debug build reports them without judging.
constexpr bool kJudged = nnh::build::flavor() != nnh::build::Flavor::Debug;

double ms(nnh::Duration d) {
    return d.seconds() * 1000.0;
}

double median(double* v, int n) {
    for (int i = 1; i < n; ++i) {
        for (int k = i; k > 0 && v[k - 1] > v[k]; --k) {
            const double t = v[k];
            v[k] = v[k - 1];
            v[k - 1] = t;
        }
    }
    return n % 2 == 1 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

} // namespace

void BenchScene::on_enter() {
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    bench_.reset(new (std::nothrow) SavesBench());
    if (bench_ && !bench_->start()) bench_.reset();
    phase_ = runs_ = 0;
    prepared_ = verdict_ = failed_run_ = false;
    failed_[0] = '\0';
    for (auto& l : lines_)
        l[0] = '\0';
    NNH_LOG_INFO(example, "saves bench: started (8 workloads: about 5 s)");
}

void BenchScene::on_exit() {
    if (bench_) bench_->finish();
    bench_.reset();
}

void BenchScene::finish_phase() {
    const Phase& p = kPhaseList[phase_];
    const int n = p.card ? kCardRuns : kMemoryRuns / kMemoryPerFrame;
    double worst = 0, sum = 0;
    for (int i = 0; i < n; ++i) {
        worst = samples_[i] > worst ? samples_[i] : worst;
        sum += samples_[i];
    }
    const double value = p.card ? median(samples_, n) : sum / n / kMemoryPerFrame;
    bool pass =
        !failed_run_ && (p.target_ms == 0 || value < p.target_ms) && (p.max_ms == 0 || worst <= p.max_ms);
    if (!kJudged) pass = !failed_run_;
    const char* verdict = failed_run_ ? "FAIL (a run failed)"
                          : !kJudged  ? "not judged (Debug)"
                          : pass      ? "PASS"
                                      : "FAIL";
    char* line = lines_[phase_];
    const std::size_t size = sizeof lines_[phase_];
    if (!p.card) {
        std::snprintf(line, size, "saves bench: %s avg %.3f ms (target < %g ms) %s", p.name, value,
                      p.target_ms, verdict);
    } else if (p.max_ms > 0) {
        std::snprintf(line, size, "saves bench: %s median %.1f ms max %.1f ms (target < %g ms, max %g ms) %s",
                      p.name, value, worst, p.target_ms, p.max_ms, verdict);
    } else if (p.target_ms > 0) {
        std::snprintf(line, size, "saves bench: %s median %.1f ms (target < %g ms) %s", p.name, value,
                      p.target_ms, verdict);
    } else {
        std::snprintf(line, size, "saves bench: %s median %.1f ms (no target)", p.name, value);
    }
    if (!pass) {
        const std::size_t at = std::strlen(failed_);
        std::snprintf(failed_ + at, sizeof failed_ - at, "%s%s", at == 0 ? "" : ", ", p.name);
    }
    NNH_LOG_INFO(example, "%s", line);
    ++phase_;
    runs_ = 0;
    prepared_ = failed_run_ = false;
}

void BenchScene::run_frame() {
    const Phase& p = kPhaseList[phase_];
    if (!prepared_) { // an untimed frame: the files the phase needs
        prepared_ = true;
        if (p.prepare != nullptr && !((*bench_).*(p.prepare))()) failed_run_ = true;
        return;
    }
    const int per_frame = p.card ? 1 : kMemoryPerFrame;
    // A load from the backup logs one warning line (FR-15): the bench times the save, not the log's sinks (on
    // the console, the remote log's network), so lines below Error are muted while it runs.
    nnh::log::set_global_level(nnh::LogLevel::Error);
    const nnh::Duration start = nnh::time::now();
    {
        NNH_PROFILE_SCOPE("saves bench");
        for (int i = 0; i < per_frame; ++i)
            failed_run_ = !((*bench_).*(p.run))() || failed_run_;
    }
    samples_[runs_] = ms(nnh::time::now() - start);
    nnh::log::set_global_level(nnh::LogLevel::Trace); // the default
    if (++runs_ >= (p.card ? kCardRuns : kMemoryRuns / kMemoryPerFrame)) finish_phase();
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::replace(*demos_.showcase);
        return;
    }
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
    if (!bench_) return;
    if (phase_ < kPhases) {
        run_frame();
    } else if (!verdict_) { // the frame after the last line: each line can be expected on its own
        verdict_ = true;
        if (failed_[0] == '\0') {
            NNH_LOG_INFO(example, "saves bench: PASS");
        } else {
            NNH_LOG_INFO(example, "saves bench: FAIL (%s)", failed_);
        }
    }
}

void BenchScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 30}, "SAVES BENCH", {.scale = 2, .align = nnh::TextAlign::Center});
    char text[64];
    if (phase_ < kPhases) {
        std::snprintf(text, sizeof text, "%s: run %d", kPhaseList[phase_].name, runs_);
    } else {
        std::snprintf(text, sizeof text, "%s", failed_[0] == '\0' ? "PASS" : "FAIL");
    }
    gfx::draw_text(nnh::Screen::Top, {200, 80}, text, {.align = nnh::TextAlign::Center});
    for (int i = 0; i < kPhases; ++i) {
        gfx::draw_text(nnh::Screen::Bottom, {4, static_cast<float>(8 + 24 * i)},
                       lines_[i] + (lines_[i][0] ? 13 : 0), {.color = nnh::rgb(200, 210, 230)});
    }
    gfx::draw_text(nnh::Screen::Bottom, {4, 226}, "SELECT showcase  START quit",
                   {.color = nnh::rgb(150, 160, 190)});
}
