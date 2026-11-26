// Cost (spec 030 T2.6, FR-19, FR-21): no allocation after the block over 100 000 frames of edits and show() calls; no
// walk over the registry while the overlay is hidden and nothing waits; a plain while (nnh::frame()) game.

#include "alloc_counter.hpp"
#include "live/live_apply.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"
#include "nnh/memory.hpp"

using namespace nnh::detail::live;
using live_test::restart;
namespace live = nnh::live;

NNH_TEST(live_cost_nothing_allocates_after_the_block) {
    if constexpr (!live::kEnabled) return;
    restart();
    float v = 0;
    live::Handle h = live::tune("c/v", &v, {.min = 0, .max = 100, .step = 1});
    live::show("c/n", 0);
    const std::uint64_t news = alloc_counter::news();
    const int allocations = nnh::mem::live_allocations();
    for (int f = 0; f < 100000; ++f) {
        if (f % 3 == 0) set_pending(*g_registry, id_slot(h.id()), float_bits(static_cast<float>(f % 100)));
        live::show("c/n", f);
        CHECK(nnh::frame());
    }
    CHECK(alloc_counter::news() == news);
    CHECK(nnh::mem::live_allocations() == allocations);
}

NNH_TEST(live_cost_no_walk_while_hidden_and_idle) {
    if constexpr (!live::kEnabled) return;
    restart();
    static float values[128];
    static live::Handle handles[128];
    for (int i = 0; i < 100; ++i) {
        char name[16];
        std::snprintf(name, sizeof name, "g%d/v%d", i % 5, i);
        handles[i] = live::tune(live::Name::runtime(name), &values[i], {.min = 0, .max = 1, .step = 1});
    }
    const std::uint32_t walks = g_registry->walks;
    for (int f = 0; f < 600; ++f) CHECK(nnh::frame());
    CHECK(g_registry->walks == walks);
    for (auto& h : handles) h = live::Handle{};
}

NNH_TEST(live_cost_a_plain_loop_game_uses_everything) {
    if constexpr (!live::kEnabled) return;
    restart();
    float speed = 1;
    bool on = false;
    int frames_seen = 0;
    live::Handle hs = live::tune("plain/speed", &speed, {.min = 0, .max = 4, .step = 0.5f});
    live::Handle ho = live::tune("plain/on", &on);
    live::Handle hw = live::watch("plain/frames", &frames_seen);
    float x = 0;
    for (int f = 0; f < 10 && nnh::frame(); ++f) {
        if (f == 4) set_pending(*g_registry, id_slot(hs.id()), float_bits(2.0f));
        x += speed;
        ++frames_seen;
        live::show("plain/x", x);
    }
    CHECK(x == 1 + 1 + 1 + 1 + 1 + 2 + 2 + 2 + 2 + 2);
    CHECK(frames_seen == 10);
}
