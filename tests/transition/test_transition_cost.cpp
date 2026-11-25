// Cost and misuse sweep (spec 023 FR-21): the state's size, no allocation over 1000 transitions of every kind, no
// log line in the normal path, every effect's draws within max_rects.

#include "nnh/detail/target_layout.hpp"
#include "alloc_counter.hpp"
#include "nnh/memory.hpp"
#include "transition/transition_internal.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;

namespace {

const tr::Effect kEffects[] = {tr::fade(), tr::wipe(tr::Edge::Bottom), tr::split(tr::SplitAxis::TopBottom),
                               tr::blinds(tr::Edge::Right, 12), tr::iris({{30, 40}}),
                               tr::iris({}, tr::IrisShape::Diamond, nnh::colors::white, 1), tr::custom()};

tr::Options options_for(int i) {
    const tr::Effect& e = kEffects[i % 7];
    return {.effect = e,
            .reveal_style = i % 2 == 0 ? tr::Reveal::Reverse : tr::Reveal::Continue,
            .tall = i % 3 == 0,
            .out = Frames{i % 4},
            .hold = Frames{i % 3},
            .in = Frames{(i + 1) % 4},
            .out_ease = static_cast<nnh::Ease>(i % nnh::kEaseCount),
            .custom = [](const tr::CustomFrame&) {},
            .loading = [] { return tr::LoadStep{}; },
            .on_covered = [] {},
            .on_finished = [] {}};
}

/// Runs transition number i: replace, push, pop or cover in turn.
void run_one(int i, Probe& a, Probe& b) {
    Probe& other = nnh::scenes::top() == &a ? b : a;
    switch (i % 4) {
    case 0: CHECK(tr::replace(other, options_for(i))); break;
    case 1: CHECK(tr::push(other, options_for(i))); break;
    case 2: CHECK(tr::pop(options_for(i))); break;
    default: CHECK(tr::cover(options_for(i))); break;
    }
    CHECK(run_until_done() > 0);
}

} // namespace

NNH_TEST(the_transition_state_stays_within_its_size) {
    CHECK(sizeof(nnh::detail::TransitionState) <= (nnh::detail::kThreeDsLayout ? 256u : 320u));
    CHECK(sizeof(nnh::detail::ResolvedEffect) <= 14u);
}

NNH_TEST(a_thousand_transitions_allocate_nothing_and_log_nothing) {
    restart({.gfx = {.max_draws = 2048}});
    Probe a, b;
    nnh::scenes::push(a);
    step();
    run_one(0, a, b); // warm up: the first frames may set things up
    LogCount lines(" transition ");
    const std::uint64_t news = alloc_counter::news();
    const int live = nnh::mem::live_allocations();
    for (int i = 1; i <= 1000; ++i) run_one(i, a, b);
    CHECK(alloc_counter::news() == news && nnh::mem::live_allocations() == live);
    CHECK(lines.count == 0);
    CHECK(nnh::gfx::stats().dropped_draws == 0);
    nnh::shutdown();
}
