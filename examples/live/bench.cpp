// The example's bench (see bench.hpp).

#include "bench.hpp"

#include <cstdio>
#include <cstring>

#include <nnh/build.hpp>
#include <nnh/debug.hpp>
#include <nnh/log.hpp>
#include <nnh/profile.hpp>
#include <nnh/time.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace {

constexpr int kFrames = 300;
constexpr int kPhases = 5;

struct PhaseInfo {
    const char* name;
    double target_ms;
};
constexpr PhaseInfo kInfo[kPhases] = {
    {"show64", 0.05}, {"hidden192", 0.02}, {"tunables", 0.5}, {"watches", 0.5}, {"register", 5.0},
};

/// The previous frame's time in the scopes whose name starts with `prefix` (several for hidden192).
double scope_ms(const char* a, const char* b) {
    nnh::profile::ScopeStats s[16];
    const int n = nnh::profile::slowest(s, 16);
    double ms = 0;
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(s[i].name, a) == 0 || (b != nullptr && std::strcmp(s[i].name, b) == 0)) {
            ms += static_cast<double>(s[i].total.ns) / 1e6;
        }
    }
    return ms;
}

double since_ms(nnh::Duration start) { return static_cast<double>((nnh::time::now() - start).ns) / 1e6; }

} // namespace

void LiveBench::start() {
    phase_ = 0;
    for (int i = 0; i < kPhases; ++i) sums_[i] = worsts_[i] = 0;
    begin_phase();
}

void LiveBench::begin_phase() {
    frame_ = 0;
    if (phase_ == 1) { // 118 + the showcase's 10 = 128 tunables, 60 + 4 = 64 watches
        for (int i = 0; i < 118; ++i) {
            char name[24];
            std::snprintf(name, sizeof name, "bench/t%d", i);
            tunables_[i] = nnh::live::tune(nnh::live::Name::runtime(name), &values_[i], {.min = 0, .max = 1, .step = 0.5f});
        }
        for (int i = 0; i < 60; ++i) {
            char name[24];
            std::snprintf(name, sizeof name, "bench/w%d", i);
            watches_[i] = nnh::live::watch(nnh::live::Name::runtime(name), &ints_[i]);
        }
        nnh::debug::set_overlay_visible(false);
    }
    if (phase_ == 2) nnh::live::show_panel(nnh::live::Page::Tunables, "bench/t0");
    if (phase_ == 3) nnh::live::show_panel(nnh::live::Page::Watches, "bench/w0");
    if (phase_ == 4) {
        nnh::debug::set_overlay_visible(false);
        for (auto& h : tunables_) h = nnh::live::Handle{};
        for (auto& h : watches_) h = nnh::live::Handle{};
    }
}

bool LiveBench::update() {
    if (phase_ < 0) return false;
    double ms = 0;
    if (phase_ == 0) {
        static char names[63][16]; // built once: the phase times show() itself, not snprintf (console run 1)
        static nnh::live::Name keys[63];
        if (frame_ == 0) {
            for (int i = 0; i < 63; ++i) {
                std::snprintf(names[i], sizeof names[i], "bench/s%d", i);
                keys[i] = nnh::live::Name::runtime(names[i]);
            }
        }
        const nnh::Duration start = nnh::time::now();
        for (int i = 0; i < 63; ++i) nnh::live::show(keys[i], i + frame_); // with spawn/alive: 64, the table's size
        ms = since_ms(start);
    } else if (phase_ == 1) {
        ms = scope_ms("live start", "live end");
    } else if (phase_ == 2 || phase_ == 3) {
        ms = scope_ms("live page", nullptr);
    } else {
        static char tnames[100][16], wnames[50][16]; // built once: the phase times registration, not snprintf
        if (frame_ == 0) {
            for (int i = 0; i < 100; ++i) std::snprintf(tnames[i], sizeof tnames[i], "bench/t%d", i);
            for (int i = 0; i < 50; ++i) std::snprintf(wnames[i], sizeof wnames[i], "bench/w%d", i);
        }
        const nnh::Duration start = nnh::time::now();
        for (int i = 0; i < 100; ++i) {
            tunables_[i] = nnh::live::tune(nnh::live::Name::runtime(tnames[i]), &values_[i], {.min = 0, .max = 1, .step = 0.5f});
        }
        for (int i = 0; i < 50; ++i) watches_[i] = nnh::live::watch(nnh::live::Name::runtime(wnames[i]), &ints_[i]);
        for (auto& h : tunables_) h = nnh::live::Handle{};
        for (auto& h : watches_) h = nnh::live::Handle{};
        ms = since_ms(start);
    }
    if (frame_ > 0) { // the first frame of a phase still measures the one before
        sums_[phase_] += ms;
        if (ms > worsts_[phase_]) worsts_[phase_] = ms;
    }
    if (++frame_ <= kFrames) return true;
    if (++phase_ < kPhases) {
        begin_phase();
        return true;
    }
    report();
    phase_ = -1;
    return false;
}

void LiveBench::report() {
    const bool judged = nnh::build::flavor() == nnh::build::Flavor::Development;
    bool pass = true;
    char failed[96] = "";
    for (int i = 0; i < kPhases; ++i) {
        const double avg = sums_[i] / kFrames;
        const bool ok = avg < kInfo[i].target_ms;
        if (!ok) {
            pass = false;
            std::strncat(failed, kInfo[i].name, sizeof failed - std::strlen(failed) - 2);
            std::strncat(failed, " ", sizeof failed - std::strlen(failed) - 1);
        }
        NNH_LOG_INFO(example, "live bench: %s avg %.4f ms worst %.4f ms (target < %.2f ms) %s", kInfo[i].name, avg,
                     worsts_[i], kInfo[i].target_ms, judged ? (ok ? "PASS" : "FAIL") : "not judged");
    }
    NNH_LOG_INFO(example, "live bench: memory %u B per handle in the game, the block at the defaults on the Stats page",
                 static_cast<unsigned>(sizeof(nnh::live::Handle)));
    if (!judged) NNH_LOG_INFO(example, "live bench: not judged (%s)", nnh::build::flavor_name());
    else if (pass) NNH_LOG_INFO(example, "live bench: PASS");
    else NNH_LOG_INFO(example, "live bench: FAIL (%s)", failed);
}
