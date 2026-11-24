#pragma once
// The transitions bench (spec 023 AC-8): its workloads and their numbers. A workload's cost per frame is the
// profiler's `transition` scope (the step and the overlay's recording) plus the `gfx submit` scope minus the base
// run's average submit (the bench scene's own drawing); the loading workload subtracts the game's `bench slice`.

#include <cstddef>
#include <nnh/transition.hpp>

enum class BenchLoad : unsigned char { Base, Fade, Wipe, Split, Blinds8, Iris, Loading, Count };

struct BenchWorkload {
    const char* name;
    double target_ms; ///< 0: not judged (the base run)
};
const BenchWorkload& bench_workload(BenchLoad load);
/// The workload's transition: a cover() whose out phase lasts kBenchFrames (Loading: covered for kBenchFrames frames,
/// each calling `slice`).
nnh::transition::Options bench_options(BenchLoad load, nnh::transition::LoadingStep slice);

inline constexpr int kBenchFrames = 300;

/// Average and worst of one workload.
struct BenchStats {
    double total_ms = 0;
    double worst_ms = 0;
    int frames = 0;
    int rects = 0; ///< the iris: the most rectangles of a frame (both screens)
    void add(double ms) {
        total_ms += ms;
        if (ms > worst_ms) worst_ms = ms;
        ++frames;
    }
    double average() const { return frames > 0 ? total_ms / frames : 0; }
};

/// "transitions bench: <name> avg X ms worst Y ms [rects R ](target < Z ms) PASS|FAIL" (or "not judged (Debug)").
void format_bench_line(char* out, std::size_t size, BenchLoad load, const BenchStats& stats, bool judged);
