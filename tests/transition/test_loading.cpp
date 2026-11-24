// Loading while covered (spec 023 FR-13): the step runs after on_covered, once per covered frame until done, its
// progress clamped (NaN → 0) and passed to the custom effect and loading_progress(); the covered phase lasts until
// the step is done and the hold has passed; a step still running at covered frame 600 warns once.

#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;

namespace {

struct Loader {
    int calls = 0;
    int slices = 5;
    int covered_calls = 0; ///< on_covered calls seen by the step
    float seen_by_custom[16] = {};
    int custom_calls = 0;
};
Loader* g_loader = nullptr;

tr::Options loading_options(Loader& l, int hold) {
    g_loader = &l;
    return {.effect = tr::custom(),
            .out = Frames{1},
            .hold = Frames{hold},
            .in = Frames{1},
            .custom =
                [](const tr::CustomFrame& f) {
                    if (f.screen == nnh::Screen::Top && f.phase == tr::Phase::Covered &&
                        g_loader->custom_calls < 16) {
                        g_loader->seen_by_custom[g_loader->custom_calls++] = f.loading;
                    }
                },
            .loading =
                [] {
                    Loader& l = *g_loader;
                    ++l.calls;
                    return tr::LoadStep{static_cast<float>(l.calls) / static_cast<float>(l.slices),
                                        l.calls == l.slices};
                },
            .on_covered = [] { ++g_loader->covered_calls; }};
}

} // namespace

NNH_TEST(the_loading_step_runs_once_per_covered_frame_until_done) {
    restart();
    Loader l;
    CHECK(tr::cover(loading_options(l, 0)));
    CHECK(tr::loading_progress() == 0.0f);
    step(); // out
    CHECK(l.calls == 0 && l.covered_calls == 0);
    step(); // covered 1: on_covered, then the first slice
    CHECK(l.covered_calls == 1 && l.calls == 1 && tr::loading_progress() == 0.2f);
    CHECK(run_until_done() == 5); // covered 2..5, then one in frame
    CHECK(l.calls == 5 && l.covered_calls == 1);
    CHECK(l.custom_calls == 5 && l.seen_by_custom[0] == 0.2f && l.seen_by_custom[4] == 1.0f);
    nnh::shutdown();
}

NNH_TEST(the_hold_keeps_the_screen_covered_after_a_quick_load) {
    restart();
    Loader l;
    l.slices = 2;
    CHECK(tr::cover(loading_options(l, 6)));
    CHECK(run_until_done() == 1 + 6 + 1);
    CHECK(l.calls == 2); // not called again once done
    nnh::shutdown();
}

NNH_TEST(loading_progress_is_clamped) {
    restart();
    static int calls = 0;
    calls = 0;
    CHECK(tr::cover({.out = Frames{0},
                     .in = Frames{0},
                     .loading = [] {
                         ++calls;
                         const float values[] = {-3.0f, 0.0f / 0.0f, 7.0f};
                         return tr::LoadStep{values[calls - 1], calls == 3};
                     }}));
    step();
    CHECK(tr::loading_progress() == 0.0f);
    step();
    CHECK(tr::loading_progress() == 0.0f);
    step();
    CHECK(tr::loading_progress() == 1.0f && !tr::running());
    nnh::shutdown();
}

NNH_TEST(a_loader_that_never_finishes_warns_once_after_ten_seconds) {
    restart();
    LogCount warned("still not done after 10 s");
    CHECK(tr::cover({.out = Frames{0}, .loading = [] { return tr::LoadStep{0.5f, false}; }}));
    step(599);
    CHECK(warned.count == 0);
    step(300);
    CHECK(warned.count == (nnh::build::kDevToolsEnabled ? 1 : 0) && tr::running());
    nnh::shutdown();
}
