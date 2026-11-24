#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/audio.hpp>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>

#include "demos.hpp"

namespace tr = nnh::transition;

namespace {

constexpr int kFirst = 3, kLast = kBenchFrames + 2; // draws that read a transition frame's scopes (the previous one)

/// The previous frame's time in every scope called `name` (scopes are keyed by the literal's address: an unoptimised
/// build may keep two copies of one name).
double scope_ms(const char* name) {
    nnh::profile::ScopeStats scopes[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(scopes, nnh::profile::kMaxScopes);
    double ms = 0;
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(scopes[i].name, name) == 0) ms += static_cast<double>(scopes[i].total.ns) / 1e6;
    }
    return ms;
}

bool judged() { return nnh::build::flavor() == nnh::build::Flavor::Development; }

/// The loading step with no work of its own: the engine's share of a loading frame is all that is measured (a busy
/// slice timed in a nested scope charged that scope's own cost to the engine, plan 023 T10.3).
tr::LoadStep bench_slice(int* slices) {
    ++*slices;
    return {static_cast<float>(*slices) / kBenchFrames, *slices >= kBenchFrames};
}

/// An empty scope: what one profiler scope costs itself, measured once a frame like the hook's two scopes.
void probe() { NNH_PROFILE_SCOPE("bench probe"); }

} // namespace

void BenchScene::on_enter() {
    load_ = 0;
    frame_ = 0;
    slices_ = 0;
    done_ = false;
    for (BenchStats& s : stats_) s = {};
    probe_ = {};
    std::strcpy(shown_, "measuring...");
    // The showcase's music would decode on the main thread between the measured scopes (evicting their code from the
    // caches): nothing of it belongs to a transition's cost.
    nnh::music::stop();
    NNH_LOG_INFO(example, "transitions bench: started");
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.rooms[0]);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void BenchScene::measure() {
    const auto load = static_cast<BenchLoad>(load_);
    const double submit = scope_ms("gfx submit");
    const double probe = scope_ms("bench probe");
    probe_.add(probe);
    BenchStats& s = stats_[load_];
    if (load == BenchLoad::Base) {
        s.add(submit);
        return;
    }
    const double base = stats_[0].average();
    if (load == BenchLoad::Rect) {
        s.add(submit - base);
        return;
    }
    // The hook opens two `transition` scopes a frame (around the step, then the overlay and the end of the frame).
    s.add(scope_ms("transition") - 2 * probe + (submit - base));
}

void BenchScene::finish_workload() {
    const auto load = static_cast<BenchLoad>(load_);
    char line[192];
    if (load == BenchLoad::Base) {
        std::snprintf(line, sizeof line,
                      "transitions bench: base gfx submit avg %.4f ms, profiler scope avg %.4f ms (taken out twice a "
                      "transition frame)",
                      stats_[0].average(), probe_.average());
    } else {
        format_bench_line(line, sizeof line, load, stats_[load_], judged());
    }
    NNH_LOG_INFO(example, "%s", line);
    if (++load_ < static_cast<int>(BenchLoad::Count)) {
        frame_ = 0;
        return;
    }
    done_ = true;
    char failed[96] = "";
    for (int i = 0; i < static_cast<int>(BenchLoad::Count); ++i) {
        const double target = bench_workload(static_cast<BenchLoad>(i)).target_ms;
        if (target > 0 && stats_[i].average() >= target) {
            std::strncat(failed, failed[0] != '\0' ? ", " : "", sizeof failed - std::strlen(failed) - 1);
            std::strncat(failed, bench_workload(static_cast<BenchLoad>(i)).name, sizeof failed - std::strlen(failed) - 1);
        }
    }
    if (!judged()) {
        std::snprintf(shown_, sizeof shown_, "transitions bench: not judged (%s)", nnh::build::flavor_name());
    } else if (failed[0] == '\0') {
        std::snprintf(shown_, sizeof shown_, "transitions bench: PASS");
    } else {
        std::snprintf(shown_, sizeof shown_, "transitions bench: FAIL (%s)", failed);
    }
    NNH_LOG_INFO(example, "%s", shown_);
    NNH_LOG_INFO(example, "transitions bench: done");
}

void BenchScene::draw() {
    if (!done_) {
        ++frame_;
        const auto load = static_cast<BenchLoad>(load_);
        if (frame_ == 1 && bench_transition(load)) {
            slices_ = 0;
            int* slices = &slices_;
            (void)tr::cover(bench_options(load, [slices] { return bench_slice(slices); }));
        }
        probe();
        if (frame_ >= kFirst && frame_ <= kLast) measure();
        if (load == BenchLoad::Rect) { // what the overlay's first rectangle costs gfx, drawn by the game
            for (nnh::Screen screen : {nnh::Screen::Top, nnh::Screen::Bottom}) {
                nnh::gfx::fill_rect(screen, {0, 0, 1, 1}, nnh::colors::black, {.layer = 15, .ignore_camera = true});
            }
        }
        if (load == BenchLoad::Iris && tr::running()) {
            const int rects = tr::effect_rects(nnh::Screen::Top, tr::iris(), tr::coverage(nnh::Screen::Top), nullptr,
                                               0) +
                              tr::effect_rects(nnh::Screen::Bottom, tr::iris(), tr::coverage(nnh::Screen::Bottom),
                                               nullptr, 0);
            if (rects > stats_[load_].rects) stats_[load_].rects = rects;
        }
        if (frame_ == kLast + 1) finish_workload();
    }
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 40}, "TRANSITIONS BENCH", {.scale = 2, .align = nnh::TextAlign::Center});
    gfx::draw_text(nnh::Screen::Bottom, {8, 8}, shown_, {.color = nnh::colors::yellow});
    gfx::draw_text(nnh::Screen::Bottom, {8, 200}, "SELECT room  START quit");
}
