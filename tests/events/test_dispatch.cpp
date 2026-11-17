// Delivery point and order of queued events (FR-3).

#include "bus_fixture.hpp"
#include "nnh/input.hpp"

using bus_test::Ping;
using bus_test::Pong;
using bus_test::restart;

namespace {

int g_order[16];
int g_count = 0;

void record(int v) {
    if (g_count < 16) g_order[g_count++] = v;
}

} // namespace

NNH_TEST(events_wait_for_the_next_frame) {
    restart();
    int received = 0;
    nnh::Subscription sub = nnh::events::subscribe<Ping>([&received](const Ping& e) { received += e.value; });
    nnh::events::publish(Ping{3});
    CHECK(received == 0); // queued, not delivered on publish
    CHECK(nnh::frame());
    CHECK(received == 3);
    CHECK(nnh::frame());
    CHECK(received == 3); // delivered once
    nnh::shutdown();
}

NNH_TEST(events_arrive_in_publish_order_across_types) {
    restart();
    g_count = 0;
    nnh::Subscription a = nnh::events::subscribe<Ping>([](const Ping& e) { record(e.value); });
    nnh::Subscription b = nnh::events::subscribe<Pong>([](const Pong& e) { record(100 + e.value); });
    nnh::events::publish(Ping{1});
    nnh::events::publish(Pong{2});
    nnh::events::publish(Ping{3});
    CHECK(nnh::frame());
    CHECK(g_count == 3 && g_order[0] == 1 && g_order[1] == 102 && g_order[2] == 3);
    nnh::shutdown();
}

NNH_TEST(each_subscriber_of_a_type_receives_the_event) {
    restart();
    int a = 0;
    int b = 0;
    nnh::Subscription sa = nnh::events::subscribe<Ping>([&a](const Ping&) { ++a; });
    nnh::Subscription sb = nnh::events::subscribe<Ping>([&b](const Ping&) { ++b; });
    nnh::Subscription other = nnh::events::subscribe<Pong>([&a](const Pong&) { a += 100; });
    nnh::events::publish(Ping{});
    CHECK(nnh::frame());
    CHECK(a == 1 && b == 1);
    nnh::shutdown();
}

NNH_TEST(handlers_run_after_input_and_inside_the_new_frame) {
    restart();
    bool saw_a = false;
    std::uint64_t frame_seen = 0;
    nnh::Subscription sub = nnh::events::subscribe<Ping>([&saw_a, &frame_seen](const Ping&) {
        saw_a = nnh::input::pressed(nnh::Button::A);
        frame_seen = nnh::frame_count();
    });
    CHECK(nnh::frame());
    nnh::events::publish(Ping{});
    nnh::platform_fake::set_held_buttons(static_cast<std::uint32_t>(nnh::Button::A));
    CHECK(nnh::frame());
    CHECK(saw_a);           // input of this frame was already read
    CHECK(frame_seen == 2); // the frame that delivers is already open
    nnh::shutdown();
}

NNH_TEST(a_subscriber_added_outside_dispatch_receives_events_still_queued) {
    restart();
    nnh::events::publish(Ping{7});
    int received = 0;
    nnh::Subscription sub = nnh::events::subscribe<Ping>([&received](const Ping& e) { received = e.value; });
    CHECK(nnh::frame());
    CHECK(received == 7);
    nnh::shutdown();
}

NNH_TEST(shutdown_drops_pending_events) {
    restart();
    nnh::events::publish(Ping{1});
    nnh::shutdown();
    CHECK(nnh::init());
    int received = 0;
    nnh::Subscription sub = nnh::events::subscribe<Ping>([&received](const Ping&) { ++received; });
    CHECK(nnh::frame());
    CHECK(received == 0);
    nnh::shutdown();
}
