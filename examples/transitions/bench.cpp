#include "bench.hpp"

#include <cstdio>

namespace tr = nnh::transition;

namespace {

constexpr BenchWorkload kWorkloads[] = {
    {"base", 0}, {"fade", 0.1}, {"wipe", 0.1}, {"split", 0.1}, {"blinds8", 0.3}, {"iris", 2.0}, {"loading engine", 0.05},
};

} // namespace

const BenchWorkload& bench_workload(BenchLoad load) { return kWorkloads[static_cast<int>(load)]; }

tr::Options bench_options(BenchLoad load, tr::LoadingStep slice) {
    tr::Options o;
    o.out = nnh::Frames{kBenchFrames};
    o.in = nnh::Frames{0};
    switch (load) {
    case BenchLoad::Wipe: o.effect = tr::wipe(tr::Edge::Left); break;
    case BenchLoad::Split: o.effect = tr::split(); break;
    case BenchLoad::Blinds8: o.effect = tr::blinds(tr::Edge::Top, 8); break;
    case BenchLoad::Iris: o.effect = tr::iris(); break; // circle, 4-pixel bands, the middle: the heaviest default
    case BenchLoad::Loading:
        o.out = nnh::Frames{0};
        o.loading = slice;
        break;
    default: o.effect = tr::fade(); break;
    }
    return o;
}

void format_bench_line(char* out, std::size_t size, BenchLoad load, const BenchStats& s, bool judged) {
    const BenchWorkload& w = bench_workload(load);
    char rects[24] = "";
    if (load == BenchLoad::Iris) std::snprintf(rects, sizeof rects, "rects %d ", s.rects);
    const char* verdict = !judged ? "not judged (Debug)" : (s.average() < w.target_ms ? "PASS" : "FAIL");
    std::snprintf(out, size, "transitions bench: %s avg %.4f ms worst %.4f ms %s(target < %.2f ms) %s", w.name,
                  s.average(), s.worst_ms, rects, w.target_ms, verdict);
}
