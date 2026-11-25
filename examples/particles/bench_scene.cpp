#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <new>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>

namespace {

const char* const kNames[BenchScene::kPhases] = {"base", "update1024", "update16x64", "emit512",
                                                 "draw512", "sprite512", "offscreen", "frame"};
// AC-7's targets as written (they predate any console measurement; the console run sets them with margin if missed).
constexpr double kTargets[BenchScene::kPhases] = {0, 0.8, 0.9, 2.35, 0, 0, 0, 0};

double scope_ms(const char* name) {
    nnh::profile::ScopeStats scopes[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(scopes, nnh::profile::kMaxScopes);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(scopes[i].name, name) == 0) return static_cast<double>(scopes[i].total.ns) / 1e6;
    }
    return 0;
}

/// Unoptimised code says nothing about the console's numbers, and only Development builds have the submit scope.
bool judged() { return nnh::build::flavor() == nnh::build::Flavor::Development; }

} // namespace

void BenchScene::on_enter() {
    bench_.reset(new (std::nothrow) ParticleBench());
    if (bench_ && !bench_->init(&sheet_)) bench_.reset();
    for (Sum& s : sums_) s = {};
    phase_ = frame_ = missed_ = culled_ = 0;
    worst_frame_ = 0;
    failed_[0] = '\0';
    done_ = false;
    NNH_LOG_INFO(example, "particles bench: started (8 runs of 300 frames, the frame test 600: about 50 s)");
    if (!bench_) NNH_LOG_ERROR(example, "particles bench: FAIL no memory");
}

// A workload's cost per frame: its own time less the timer's (the base run times an empty call: two time::now()
// reads, each a system tick and a 64-bit division on the 3DS), plus its share of gfx submit (spec 024 T10.3).
double BenchScene::cost(Phase p) const {
    const Sum& s = sums_[p];
    const Sum& b = sums_[kBase];
    const double n = s.n > 0 ? s.n : 1, bn = b.n > 0 ? b.n : 1;
    return (s.own / n - b.own / bn) + (s.submit / n - b.submit / bn);
}

void BenchScene::finish() {
    const Phase p = static_cast<Phase>(phase_);
    const char* judge = judged() ? "" : " not judged (Debug)";
    char line[128] = {};
    bool pass = true;
    if (p == kBase) {
        const double n = sums_[p].n > 0 ? sums_[p].n : 1;
        std::snprintf(line, sizeof line, "particles bench: base gfx submit avg %.4f ms timer %.4f ms",
                      sums_[p].submit / n, sums_[p].own / n);
    } else if (p == kDraw512) {
        std::snprintf(line, sizeof line, "particles bench: draw512 avg %.3f ms", cost(p));
    } else if (p == kSprite512) {
        const double particle_us = cost(kDraw512) * 1000.0 / ParticleBench::kDrawn;
        const double sprite_us = cost(kSprite512) * 1000.0 / ParticleBench::kDrawn;
        const double ratio = sprite_us > 0 ? particle_us / sprite_us : 0;
        pass = ratio <= 0.75;
        std::snprintf(line, sizeof line, "particles bench: draw particle %.2f us sprite %.2f us ratio %.2f (target <= 0.75)%s %s",
                      particle_us, sprite_us, ratio, judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else if (p == kOffscreen) {
        const double submit = cost(p);
        pass = submit < 0.06 && culled_ == ParticleBench::kDrawn;
        std::snprintf(line, sizeof line, "particles bench: offscreen submit %.3f ms culled %d%s %s", submit, culled_,
                      judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else if (p == kFrame) {
        pass = missed_ == 0;
        std::snprintf(line, sizeof line, "particles bench: frame worst %.1f ms missed %d of %d%s %s", worst_frame_,
                      missed_, kFrameTestFrames, judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else {
        const double avg = cost(p);
        pass = avg < kTargets[p];
        std::snprintf(line, sizeof line, "particles bench: %s avg %.3f ms worst %.3f ms (target < %g ms)%s %s",
                      kNames[p], avg, sums_[p].own_worst, kTargets[p], judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    }
    if (judged() && !pass) {
        const std::size_t at = std::strlen(failed_);
        std::snprintf(failed_ + at, sizeof failed_ - at, "%s%s", at == 0 ? "" : ", ", kNames[p]);
    }
    NNH_LOG_INFO(example, "%s", line);
    ++phase_;
    frame_ = 0;
}

void BenchScene::verdict() {
    done_ = true;
    if (failed_[0] == '\0') {
        NNH_LOG_INFO(example, "particles bench: PASS");
    } else {
        NNH_LOG_INFO(example, "particles bench: FAIL (%s)", failed_);
    }
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::replace(*demos_.showcase);
        return;
    }
    if (nnh::input::pressed(nnh::Button::Start)) {
        nnh::scenes::pop();
        return;
    }
    if (!bench_ || done_) return;
    if (phase_ >= kPhases) {
        verdict(); // the frame after the last line: each line can be expected on its own
        return;
    }
    Sum& s = sums_[phase_];
    // The previous frame's submit belongs to this phase from its second frame on.
    if (frame_ >= 2) {
        s.submit += scope_ms("gfx submit");
        ++s.n;
    }
    nnh::gfx::set_camera(nnh::Screen::Top, {.position = phase_ == kOffscreen ? nnh::Vec2{5000, 5000} : nnh::Vec2{}});
    if (phase_ == kEmit512) bench_->clear_emit512(); // untimed
    const nnh::Duration start = nnh::time::now();
    switch (phase_) {
    case kUpdate1024: bench_->update1024(); break;
    case kUpdate16x64: bench_->update16x64(); break;
    case kEmit512: (void)bench_->emit512(); break;
    case kDraw512: bench_->draw512(); break;
    case kSprite512: bench_->sprite512(); break;
    case kOffscreen: bench_->offscreen(); break;
    case kFrame: bench_->frame(frame_); break;
    default: break;
    }
    const double ms = (nnh::time::now() - start).seconds() * 1000.0;
    if (frame_ >= 2) {
        s.own += ms;
        if (ms > s.own_worst) s.own_worst = ms;
    }
    if (phase_ == kOffscreen) culled_ = bench_->culled();
    if (phase_ == kFrame && frame_ >= 2) {
        const double delta = nnh::time::delta().seconds() * 1000.0;
        if (delta > worst_frame_) worst_frame_ = delta;
        if (delta > 20) ++missed_;
    }
    if (++frame_ >= (phase_ == kFrame ? kFrameTestFrames : kFrames)) finish();
}

void BenchScene::draw() {
    namespace gfx = nnh::gfx;
    char line[64];
    std::snprintf(line, sizeof line, "%s  %d / %d", phase_ < kPhases ? kNames[phase_] : "done", frame_,
                  phase_ == kFrame ? kFrameTestFrames : kFrames);
    gfx::draw_text(nnh::Screen::Bottom, {8, 8}, "PARTICLES BENCH", {.scale = 2});
    gfx::draw_text(nnh::Screen::Bottom, {8, 40}, line);
    gfx::draw_text(nnh::Screen::Bottom, {8, 60}, "the numbers are in the log\nSELECT showcase  START quit",
                   {.color = nnh::colors::gray});
}
