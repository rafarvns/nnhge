// Event bus capacities: defaults, AppConfig::events, memory tag, no allocation per publish (FR-5).

#include "bus_fixture.hpp"
#include "nnh/memory.hpp"

namespace nnh {
NNH_MEM_TAG_EXTERN(events);
}

using bus_test::Ping;
using bus_test::restart;

NNH_TEST(default_capacities_are_16kb_and_256_subscribers) {
    restart();
    const nnh::events::Stats s = nnh::events::stats();
    CHECK(s.queue_bytes == 16 * 1024);
    CHECK(s.max_subscribers == 256);
    CHECK(s.subscribers == 0 && s.dropped_events == 0);
    nnh::shutdown();
}

NNH_TEST(capacities_come_from_app_config) {
    restart({.events = {.queue_bytes = 32 * 1024, .max_subscribers = 512}});
    CHECK(nnh::events::stats().queue_bytes == 32 * 1024);
    CHECK(nnh::events::stats().max_subscribers == 512);
    nnh::shutdown();
}

NNH_TEST(capacities_below_the_minimum_are_raised) {
    restart({.events = {.queue_bytes = 10, .max_subscribers = 0}});
    CHECK(nnh::events::stats().queue_bytes == 1024);
    CHECK(nnh::events::stats().max_subscribers == 8);
    nnh::shutdown();
}

NNH_TEST(bus_memory_is_tagged_events_and_freed_at_shutdown) {
    restart({.events = {.queue_bytes = 8 * 1024, .max_subscribers = 64}});
    const std::size_t bytes = nnh::mem::tag_stats(nnh::NNH_MEM_TAG_REF(events)).bytes;
    CHECK(bytes >= 8 * 1024 + 64 * 16);
    nnh::shutdown();
    CHECK(nnh::mem::tag_stats(nnh::NNH_MEM_TAG_REF(events)).bytes == 0);
}

NNH_TEST(publishing_and_dispatching_never_allocate) {
    restart();
    int received = 0;
    nnh::Subscription sub = nnh::events::subscribe<Ping>([&received](const Ping&) { ++received; });
    const int live = nnh::mem::live_allocations();
    for (int frame = 0; frame < 10; ++frame) {
        for (int i = 0; i < 20; ++i) nnh::events::publish(Ping{i});
        CHECK(nnh::frame());
    }
    CHECK(received == 200);
    CHECK(nnh::mem::live_allocations() == live);
    nnh::shutdown();
}

NNH_TEST(init_fails_cleanly_without_memory_for_the_bus) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    nnh::platform_fake::set_region_capacity(nnh::MemRegion::Heap, 4 * 1024);
    CHECK(!nnh::init({.events = {.queue_bytes = 64 * 1024}}));
    CHECK(nnh::mem::tag_stats(nnh::NNH_MEM_TAG_REF(events)).bytes == 0);
    CHECK(nnh::platform_fake::state().shutdown_calls == 1);
    nnh::platform_fake::reset();
}
