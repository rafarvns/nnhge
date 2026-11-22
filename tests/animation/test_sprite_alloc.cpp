// No allocation while playing (T5.3; FR-15, AC-3): after the library is built, playing, queueing, updating, markers,
// callbacks and state switches on 512 animators allocate nothing (C++ heap and nnh::mem); the tag is clean after.

#include "alloc_counter.hpp"
#include "library_fixture.hpp"
#include "nnh/memory.hpp"

namespace counter = nnh::test::alloc_counter;
using nnh::SpriteAnimator;
using nnh::SpriteStateId;

namespace {

struct World {
    nnh::SpriteLibrary lib = anim_test::strip(8);
    nnh::SpriteClipId walk{}, jump{}, pp{};
    SpriteStateId idle{}, run{}, hop{};
    SpriteAnimator animators[512];
    int markers = 0;

    World() {
        walk = anim_test::clip(lib, "walk", 4, 2);
        jump = anim_test::clip(lib, "jump", 3, 1, nnh::SpriteMode::Once);
        pp = anim_test::clip(lib, "pp", 4, 1, nnh::SpriteMode::PingPong, 2);
        CHECK(lib.add_marker(walk, 1, "step").ok());
        CHECK(lib.add_marker(pp, 3, "turn").ok());
        idle = lib.add_state("idle", pp).value();
        run = lib.add_state("run", walk).value();
        hop = lib.add_state("hop", jump).value();
        CHECK(lib.set_next_on_finish(hop, idle).ok());
        CHECK(lib.add_transition({.from = run, .to = idle, .when = nnh::SpriteSwitch::AtEnd}).ok());
    }

    void play() {
        for (int i = 0; i < 512; ++i) {
            SpriteAnimator& a = animators[i];
            a = i % 2 == 0 ? SpriteAnimator{lib, idle} : SpriteAnimator{lib};
            a.on_notice([w = this](SpriteAnimator&, const nnh::SpriteNotice&) { ++w->markers; });
            if (i % 2 == 1) {
                a.play(i % 4 == 1 ? walk : jump);
                (void)a.queue(pp);
                a.set_speed(i % 3 == 0 ? 1.5f : 0.5f);
            }
        }
        for (int frame = 0; frame < 120; ++frame) {
            for (int i = 0; i < 512; ++i) {
                SpriteAnimator& a = animators[i];
                if (i % 2 == 0) a.request((frame / 10 + i) % 3 == 0 ? run : ((frame / 7) % 4 == 0 ? hop : idle));
                (void)a.update();
            }
        }
    }
};

} // namespace

NNH_TEST(playing_512_animators_allocates_nothing) {
    const int live_before = nnh::mem::live_allocations();
    const nnh::MemStats tag_before = nnh::mem::tag_stats(nnh::detail::animation_memory_tag());
    {
        auto* w = new World; // 512 animators: kept off the test's stack; allocated before counting starts
        const int live = nnh::mem::live_allocations();
        CHECK(live == live_before + 1); // the library's one block
        counter::reset();
        counter::enable(true);
        w->play();
        counter::enable(false);
        CHECK(counter::count() == 0);
        CHECK(nnh::mem::live_allocations() == live);
        CHECK(w->markers > 1000);
        delete w;
    }
    CHECK(nnh::mem::live_allocations() == live_before);
    CHECK(nnh::mem::tag_stats(nnh::detail::animation_memory_tag()).bytes == tag_before.bytes);
}
