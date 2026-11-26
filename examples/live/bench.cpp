// The example's bench (see bench.hpp).

#include "bench.hpp"

#include <cstdio>
#include <memory>
#include <new>

#include <nnh/build.hpp>
#include <nnh/live.hpp>
#include <nnh/log.hpp>
#include <nnh/time.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace {

constexpr int kRounds = 300;

struct Phase {
    const char* name;
    double target_ms;
    double avg = 0, worst = 0;
};

struct Values {
    float f[100] = {};
    int w[50] = {};
    nnh::live::Handle tunables[100];
    nnh::live::Handle watches[50];
};

void time_show(Phase& p) {
    for (int r = 0; r < kRounds; ++r) {
        const nnh::Duration start = nnh::time::now();
        for (int i = 0; i < 63; ++i) { // with the showcase's spawn/alive: 64, the table's size
            char name[16];
            std::snprintf(name, sizeof name, "bench/s%d", i);
            nnh::live::show(nnh::live::Name::runtime(name), i + r);
        }
        const double ms = static_cast<double>((nnh::time::now() - start).ns) / 1e6;
        p.avg += ms / kRounds;
        if (ms > p.worst) p.worst = ms;
    }
}

void time_register(Phase& p, Values& v) {
    for (int r = 0; r < 20; ++r) {
        const nnh::Duration start = nnh::time::now();
        for (int i = 0; i < 100; ++i) {
            char name[16];
            std::snprintf(name, sizeof name, "bench/t%d", i);
            v.tunables[i] = nnh::live::tune(nnh::live::Name::runtime(name), &v.f[i], {.min = 0, .max = 1, .step = 0.5f});
        }
        for (int i = 0; i < 50; ++i) {
            char name[16];
            std::snprintf(name, sizeof name, "bench/w%d", i);
            v.watches[i] = nnh::live::watch(nnh::live::Name::runtime(name), &v.w[i]);
        }
        for (auto& h : v.tunables) h = nnh::live::Handle{};
        for (auto& h : v.watches) h = nnh::live::Handle{};
        const double ms = static_cast<double>((nnh::time::now() - start).ns) / 1e6;
        p.avg += ms / 20;
        if (ms > p.worst) p.worst = ms;
    }
}

} // namespace

void run_live_bench() {
    Phase phases[] = {{"show64", 0.05}, {"register150", 5.0}};
    time_show(phases[0]);
    std::unique_ptr<Values> v(new (std::nothrow) Values());
    if (v != nullptr) time_register(phases[1], *v);
    const bool judged = nnh::build::flavor() != nnh::build::Flavor::Debug;
    bool pass = true;
    for (const Phase& p : phases) {
        const bool ok = p.avg < p.target_ms;
        pass = pass && ok;
        NNH_LOG_INFO(example, "live bench: %s avg %.4f ms worst %.4f ms (target < %.2f ms) %s", p.name, p.avg, p.worst,
                     p.target_ms, judged ? (ok ? "PASS" : "FAIL") : "not judged (Debug)");
    }
    if (!judged) NNH_LOG_INFO(example, "live bench: not judged (Debug)");
    else NNH_LOG_INFO(example, "live bench: %s", pass ? "PASS" : "FAIL");
}
