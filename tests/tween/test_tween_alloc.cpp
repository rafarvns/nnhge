// No allocation while playing (spec 015 FR-4, AC-4; T4.4): after a set is created, starting, building, playing,
// finishing and killing tweens, timers and sequences allocate nothing (C++ heap and nnh::mem); the leak report is
// clean once the sets are gone.

#include "alloc_counter.hpp"
#include "nnh/gfx.hpp"
#include "nnh/memory.hpp"
#include "tween_fixture.hpp"

using nnh::Frames;
using nnh::TweenHandle;
using nnh::TweenSet;
namespace counter = nnh::test::alloc_counter;

namespace {

struct Game {
    TweenSet set{{.tweens = 64, .steps = 32}};
    float a = 0;
    nnh::Vec2 b{};
    nnh::Color c{};
    int calls = 0;
};

void play(Game& g) {
    for (int round = 0; round < 20; ++round) {
        const TweenHandle t = g.set.to(&g.a, 1.0f, Frames{3}, {.ease = nnh::Ease::ElasticOut, .loops = 2});
        g.set.by(&g.b, {1, 2}, Frames{2}, {.loop_mode = nnh::LoopMode::Yoyo, .on_finish = [p = &g] { ++p->calls; }});
        g.set.from_to(nnh::Setter<nnh::Color>{[p = &g](nnh::Color v) { p->c = v; }}, nnh::Color{}, nnh::rgb(9, 9, 9),
                      Frames{4});
        g.set.after(Frames{2}, [p = &g] { ++p->calls; });
        const TweenHandle tick = g.set.every(Frames{1}, [p = &g] { ++p->calls; }, 3);
        const TweenHandle seq = g.set.sequence({.loops = 2, .loop_mode = nnh::LoopMode::Yoyo})
                                    .to(&g.a, 2.0f, Frames{2})
                                    .join()
                                    .to(&g.b, {3, 3}, Frames{3})
                                    .wait(Frames{1})
                                    .call([p = &g] { ++p->calls; });
        tween_test::run(g.set, 3);
        g.set.kill(t);
        g.set.complete(tick);
        g.set.pause(seq);
        g.set.resume(seq);
        tween_test::run(g.set, 20);
        g.set.kill_all();
    }
}

} // namespace

NNH_TEST(the_alloc_counter_counts_operator_new) {
    counter::reset();
    counter::enable(true);
    static int* volatile kept = nullptr; // volatile: the optimizer may not elide the new/delete pair
    kept = new int(3);
    counter::enable(false);
    delete kept;
    CHECK(counter::count() == 1);
}

NNH_TEST(playing_tweens_timers_and_sequences_allocates_nothing) {
    const int live_before = nnh::mem::live_allocations();
    const std::size_t tag_before = nnh::mem::tag_stats(nnh::detail::tween_memory_tag()).bytes;
    {
        Game g;
        const int live = nnh::mem::live_allocations();
        CHECK(live == live_before + 1); // the set's one block
        counter::reset();
        counter::enable(true);
        play(g);
        counter::enable(false);
        CHECK(counter::count() == 0);
        CHECK(nnh::mem::live_allocations() == live);
        CHECK(g.calls > 0 && g.set.active() == 0);
    }
    CHECK(nnh::mem::live_allocations() == live_before);
    CHECK(nnh::mem::tag_stats(nnh::detail::tween_memory_tag()).bytes == tag_before); // nothing left behind
}
