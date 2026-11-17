// nnh::Subscription: RAII, move, reset, stale handles, member-function handlers (FR-2).

#include <utility>

#include "bus_fixture.hpp"

using bus_test::Ping;
using bus_test::restart;

namespace {

struct Listener {
    int total = 0;
    void on_ping(const Ping& e) { total += e.value; }
};

struct ConstListener {
    int* out;
    void on_ping(const Ping& e) const { *out += e.value; }
};

int g_free_calls = 0;
void free_handler(const Ping&) { ++g_free_calls; }

} // namespace

NNH_TEST(destroying_a_subscription_unsubscribes) {
    restart();
    int received = 0;
    {
        nnh::Subscription sub = nnh::events::subscribe<Ping>([&received](const Ping&) { ++received; });
        CHECK(sub.active());
        CHECK(nnh::events::stats().subscribers == 1);
    }
    CHECK(nnh::events::stats().subscribers == 0);
    nnh::events::publish(Ping{1});
    CHECK(nnh::frame());
    CHECK(received == 0);
    nnh::shutdown();
}

NNH_TEST(reset_unsubscribes_and_is_idempotent) {
    restart();
    nnh::Subscription sub = nnh::events::subscribe<Ping>([](const Ping&) {});
    sub.reset();
    CHECK(!sub.active());
    sub.reset();
    CHECK(nnh::events::stats().subscribers == 0);
    nnh::shutdown();
}

NNH_TEST(moving_transfers_the_subscription) {
    restart();
    int received = 0;
    nnh::Subscription a = nnh::events::subscribe<Ping>([&received](const Ping&) { ++received; });
    nnh::Subscription b = std::move(a);
    CHECK(!a.active() && b.active());
    a.reset(); // the moved-from handle must not end b's subscription
    CHECK(b.active());
    nnh::Subscription c;
    c = std::move(b);
    CHECK(c.active() && nnh::events::stats().subscribers == 1);
    nnh::events::publish(Ping{1});
    CHECK(nnh::frame());
    CHECK(received == 1);
    nnh::shutdown();
}

NNH_TEST(move_assignment_ends_the_previous_subscription) {
    restart();
    nnh::Subscription a = nnh::events::subscribe<Ping>([](const Ping&) {});
    nnh::Subscription b = nnh::events::subscribe<Ping>([](const Ping&) {});
    a = std::move(b);
    CHECK(nnh::events::stats().subscribers == 1);
    nnh::shutdown();
}

NNH_TEST(stale_handles_after_shutdown_do_not_touch_a_new_engine) {
    restart();
    nnh::Subscription old = nnh::events::subscribe<Ping>([](const Ping&) {});
    nnh::shutdown();
    CHECK(!old.active());
    CHECK(nnh::init());
    int received = 0;
    nnh::Subscription fresh = nnh::events::subscribe<Ping>([&received](const Ping&) { ++received; });
    old.reset(); // same slot index as `fresh`, but from the previous engine
    CHECK(fresh.active());
    nnh::events::publish(Ping{1});
    CHECK(nnh::frame());
    CHECK(received == 1);
    nnh::shutdown();
}

NNH_TEST(member_functions_and_free_functions_can_be_handlers) {
    restart();
    Listener listener;
    int const_total = 0;
    ConstListener const_listener{&const_total};
    g_free_calls = 0;
    nnh::Subscription a = nnh::events::subscribe<&Listener::on_ping>(&listener);
    nnh::Subscription b = nnh::events::subscribe<&ConstListener::on_ping>(&const_listener);
    nnh::Subscription c = nnh::events::subscribe<Ping>(&free_handler);
    nnh::events::publish(Ping{5});
    CHECK(nnh::frame());
    CHECK(listener.total == 5 && const_total == 5 && g_free_calls == 1);
    nnh::shutdown();
}

NNH_TEST(mutable_lambdas_keep_their_state_between_events) {
    restart();
    int last = 0;
    nnh::Subscription sub = nnh::events::subscribe<Ping>([count = 0, out = &last](const Ping&) mutable {
        *out = ++count;
    });
    nnh::events::publish(Ping{});
    nnh::events::publish(Ping{});
    CHECK(nnh::frame());
    CHECK(last == 2);
    nnh::shutdown();
}
