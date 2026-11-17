// Overflowing a capacity: assert in Debug/Development, drop + log in Release (FR-6).

#include <cstring>

#include "bus_fixture.hpp"
#include "nnh/build.hpp"

using bus_test::Ping;
using bus_test::restart;

namespace {

nnh::Subscription g_subs[16];

void overfill_queue() {
    for (int i = 0; i < 1000; ++i) nnh::events::publish(Ping{i});
}

void overfill_subscribers() {
    for (nnh::Subscription& s : g_subs) s = nnh::events::subscribe<Ping>([](const Ping&) {});
}

bool last_log_mentions(const char* text) {
    return std::strstr(nnh::platform_fake::state().last_console_line, text) != nullptr;
}

} // namespace

NNH_TEST(a_full_queue_asserts_or_drops_and_logs) {
    restart({.events = {.queue_bytes = 1024}});
    const bool crashed = nnh::platform_fake::run_expecting_crash(overfill_queue);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (!nnh::build::kAssertsEnabled) {
        CHECK(nnh::events::stats().dropped_events > 0);
        CHECK(last_log_mentions("queue full"));
        int received = 0;
        nnh::Subscription sub = nnh::events::subscribe<Ping>([&received](const Ping&) { ++received; });
        CHECK(nnh::frame());
        CHECK(received > 0 && received < 1000); // what fit is still delivered
    }
    nnh::shutdown();
}

NNH_TEST(a_full_subscriber_table_asserts_or_returns_an_empty_subscription) {
    restart({.events = {.max_subscribers = 8}});
    const bool crashed = nnh::platform_fake::run_expecting_crash(overfill_subscribers);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (!nnh::build::kAssertsEnabled) {
        CHECK(nnh::events::stats().subscribers == 8);
        CHECK(!g_subs[8].active());
        CHECK(last_log_mentions("subscriber table full"));
    }
    for (nnh::Subscription& s : g_subs) s.reset();
    nnh::shutdown();
}

NNH_TEST(drops_are_logged_once_per_frame) {
    if (nnh::build::kAssertsEnabled) return; // Debug/Development stop at the first overflow
    restart({.events = {.queue_bytes = 1024}});
    const int before = nnh::platform_fake::state().console_lines;
    overfill_queue();
    overfill_queue();
    CHECK(nnh::platform_fake::state().console_lines == before + 1);
    nnh::shutdown();
}
