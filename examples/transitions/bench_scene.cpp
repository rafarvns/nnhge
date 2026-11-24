#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>

#include "demos.hpp"

namespace tr = nnh::transition;

namespace {

constexpr int kFirst = 3, kLast = kBenchFrames + 2; // draws that read a transition frame's scopes (the previous one)

double scope_ms(const char* name) {
    nnh::profile::ScopeStats scopes[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(scopes, nnh::profile::kMaxScopes);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(scopes[i].name, name) == 0) return static_cast<double>(scopes[i].total.ns) / 1e6;
    }
    return 0;
}

bool judged() { return nnh::build::flavor() == nnh::build::Flavor::Development; }

/// A fixed integer loop: the game's share of a loading frame, timed apart.
tr::LoadStep bench_slice(int* slices) {
    NNH_PROFILE_SCOPE("bench slice");
    std::uint32_t x = static_cast<std::uint32_t>(*slices);
    for (int i = 0; i < 5000; ++i) x = x * 1664525u + 1013904223u;
    ++*slices;
    return {static_cast<float>(*slices) / kBenchFrames, *slices >= kBenchFrames || x == 0};
}

} // namespace

void BenchScene::on_enter() {
    load_ = 0;
    frame_ = 0;
    slices_ = 0;
    done_ = false;
    for (BenchStats& s : stats_) s = {};
    std::strcpy(shown_, "measuring...");
    NNH_LOG_INFO(example, "transitions bench: started");
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.rooms[0]);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void BenchScene::measure() {
    const auto load = static_cast<BenchLoad>(load_);
    const double submit = scope_ms("gfx submit");
    BenchStats& s = stats_[load_];
    if (load == BenchLoad::Base) {
        s.add(submit);
        return;
    }
    const double base = stats_[0].average();
    const double slice = load == BenchLoad::Loading ? scope_ms("bench slice") : 0;
    s.add(scope_ms("transition") + (submit - base) - slice);
}

void BenchScene::finish_workload() {
    const auto load = static_cast<BenchLoad>(load_);
    char line[160];
    if (load == BenchLoad::Base) {
        std::snprintf(line, sizeof line, "transitions bench: base gfx submit avg %.4f ms", stats_[0].average());
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
    for (int i = 1; i < static_cast<int>(BenchLoad::Count); ++i) {
        if (stats_[i].average() >= bench_workload(static_cast<BenchLoad>(i)).target_ms) {
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
        if (frame_ == 1 && load != BenchLoad::Base) {
            slices_ = 0;
            int* slices = &slices_;
            (void)tr::cover(bench_options(load, [slices] { return bench_slice(slices); }));
        }
        if (frame_ >= kFirst && frame_ <= kLast) measure();
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
