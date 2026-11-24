// Cost rules (spec 024 FR-23, FR-24; T3.6): after creation nothing allocates over 100 000 mixed operations, nothing is
// logged in the normal path and no event is published.

#include <cstring>

#include "alloc_counter.hpp"
#include "nnh/events.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "particles_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;

namespace {

int g_lines = 0;
void count_lines(nnh::LogLevel, const char*, void*) { ++g_lines; }

} // namespace

NNH_TEST(particles_allocate_log_and_publish_nothing_while_playing) {
    ParticleDesc d = particles_test::moving_desc();
    d.rate = 200;
    d.shape = nnh::ParticleShape::Box;
    d.box = {10, 4};
    d.damping = 0.5f;
    d.spin = {-2, 2};
    d.color_keys[0] = {0, nnh::colors::yellow};
    d.color_keys[1] = {1, nnh::rgba(255, 0, 0, 0)};
    d.bursts[0] = {.count = {1, 20}, .cycles = 0, .interval = nnh::Frames{3}};
    ParticleEmitter e = particles_test::make(d, 2048);
    const auto news = alloc_counter::news();
    const int live = nnh::mem::live_allocations();
    const nnh::events::Stats events = nnh::events::stats();
    CHECK(nnh::log::add_sink(count_lines));
    g_lines = 0;
    for (int op = 0; op < 100000; ++op) {
        switch (op % 7) {
        case 0: e.update(); break;
        case 1: e.emit_at(nnh::Vec2{static_cast<float>(op % 97), 3}, op % 4); break;
        case 2: e.set_position({static_cast<float>(op % 50), 2}); break;
        case 3: if (e.alive() > 0) (void)e.particle(e.alive() - 1); break;
        case 4: (void)e.stats(); break;
        case 5: if (op % 700 == 5) e.restart(); break;
        default: if (op % 1400 == 6) e.clear(); break;
        }
    }
    nnh::log::remove_sink(count_lines);
    CHECK(alloc_counter::news() == news && nnh::mem::live_allocations() == live);
    CHECK(g_lines == 0);
    const nnh::events::Stats after = nnh::events::stats();
    CHECK(after.queue_bytes == events.queue_bytes && after.subscribers == events.subscribers);
    CHECK(after.dropped_events == events.dropped_events);
}
