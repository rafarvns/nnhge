// Re-entrancy rules of the event bus, one test per rule (FR-4, docs/architecture/events.md).

#include "bus_fixture.hpp"

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

NNH_TEST(events_published_by_handlers_arrive_in_the_same_dispatch_after_queued_ones) {
    restart();
    g_count = 0;
    nnh::Subscription ping = nnh::events::subscribe<Ping>([](const Ping& e) {
        record(e.value);
        if (e.value == 1) nnh::events::publish(Pong{1});
    });
    nnh::Subscription pong = nnh::events::subscribe<Pong>([](const Pong& e) { record(100 + e.value); });
    nnh::events::publish(Ping{1});
    nnh::events::publish(Ping{2});
    CHECK(nnh::frame());
    CHECK(g_count == 3 && g_order[0] == 1 && g_order[1] == 2 && g_order[2] == 101);
    nnh::shutdown();
}

NNH_TEST(a_handler_removed_during_dispatch_is_not_called_again) {
    restart();
    int second_calls = 0;
    nnh::Subscription second;
    nnh::Subscription first = nnh::events::subscribe<Ping>([&second](const Ping&) { second.reset(); });
    second = nnh::events::subscribe<Ping>([&second_calls](const Ping&) { ++second_calls; });
    nnh::events::publish(Ping{});
    nnh::events::publish(Ping{}); // already queued when `second` is removed
    CHECK(nnh::frame());
    CHECK(second_calls == 0);
    nnh::shutdown();
}

NNH_TEST(a_handler_can_unsubscribe_itself_and_keep_using_its_captures) {
    restart();
    struct Self {
        nnh::Subscription sub;
        int value_after_reset = 0;
        int calls = 0;
    } self;
    Self* s = &self;
    const int marker = 42;
    self.sub = nnh::events::subscribe<Ping>([s, marker](const Ping&) {
        s->sub.reset();
        ++s->calls;
        s->value_after_reset = marker; // captures still intact after the slot was cleared
    });
    nnh::events::publish(Ping{});
    nnh::events::publish(Ping{});
    CHECK(nnh::frame());
    CHECK(self.calls == 1 && self.value_after_reset == 42);
    nnh::shutdown();
}

NNH_TEST(a_handler_that_replaces_itself_in_its_own_slot_is_not_overwritten) {
    restart();
    struct State {
        nnh::Subscription sub;
        int old_calls = 0;
        int new_calls = 0;
    } state;
    State* s = &state;
    state.sub = nnh::events::subscribe<Ping>([s](const Ping&) {
        ++s->old_calls;
        s->sub.reset(); // frees this slot...
        s->sub = nnh::events::subscribe<Ping>([s](const Ping&) { ++s->new_calls; }); // ...which is reused
    });
    nnh::events::publish(Ping{});
    CHECK(nnh::frame());
    nnh::events::publish(Ping{});
    CHECK(nnh::frame());
    CHECK(state.old_calls == 1 && state.new_calls == 1);
    nnh::shutdown();
}

NNH_TEST(a_handler_added_during_dispatch_only_receives_later_publishes) {
    restart();
    int late_calls = 0;
    int* late = &late_calls;
    nnh::Subscription added;
    nnh::Subscription* slot = &added;
    nnh::Subscription adder = nnh::events::subscribe<Ping>([slot, late](const Ping& e) {
        if (e.value == 1) {
            *slot = nnh::events::subscribe<Ping>([late](const Ping&) { ++*late; });
            nnh::events::publish(Ping{3}); // published after subscribing: delivered to it
        }
    });
    nnh::events::publish(Ping{1});
    nnh::events::publish(Ping{2}); // already queued: not delivered to the new handler
    CHECK(nnh::frame());
    CHECK(late_calls == 1);
    nnh::shutdown();
}

NNH_TEST(the_depth_limit_stops_feedback_loops) {
    restart();
    int deliveries = 0;
    nnh::Subscription loop = nnh::events::subscribe<Ping>([&deliveries](const Ping& e) {
        ++deliveries;
        nnh::events::publish(Ping{e.value + 1}); // every ping causes another one
    });
    nnh::events::publish(Ping{0});
    CHECK(nnh::frame()); // returns: the chain is cut
    CHECK(deliveries == nnh::events::kMaxDepth + 1);
    CHECK(nnh::events::stats().dropped_events == 1);
    CHECK(nnh::frame());
    CHECK(deliveries == nnh::events::kMaxDepth + 1); // nothing left over for the next frame
    nnh::shutdown();
}
