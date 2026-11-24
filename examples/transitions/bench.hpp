#pragma once
// The transitions bench (spec 023 AC-8): its workloads and their numbers. A workload's cost per frame is the
// profiler's `transition` scopes (the step, the overlay's recording, the end of the frame) minus their own cost (two
// scopes a frame, each costing what the empty `bench probe` scope of the same frame measures), plus the `gfx submit`
// scope minus the base run's average submit (the bench scene's own drawing). The loading workload's step does no work
// of its own, so nothing of the game's is in it. The `rect` run (no transition) shows the gfx share of one solid
// rectangle per screen over the scene's text — the cost every overlay pays first — and is not judged
// (plan 023 "Optimisation round (T10.3)").

#include <cstddef>
#include <nnh/transition.hpp>

enum class BenchLoad : unsigned char { Base, Rect, Fade, Wipe, Split, Blinds8, Iris, Loading, Count };

struct BenchWorkload {
    const char* name;
    double target_ms; ///< 0: not judged (the base and rect runs)
};
const BenchWorkload& bench_workload(BenchLoad load);
/// True for the workloads that run a transition (all but Base and Rect).
bool bench_transition(BenchLoad load);
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

/// "transitions bench: <name> avg X ms worst Y ms [rects R ](target < Z ms) PASS|FAIL" (or "not judged (Debug)");
/// the rect run: "transitions bench: rect avg X ms worst Y ms (gfx share, one solid rectangle per screen, not judged)".
void format_bench_line(char* out, std::size_t size, BenchLoad load, const BenchStats& stats, bool judged);
