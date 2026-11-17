// Subscriber slots with generations and start sequences (events/subscriber_table.hpp).

#include "events/subscriber_table.hpp"
#include "nnh/detail/type_id.hpp"
#include "test.hpp"

using nnh::detail::SubscriberSlot;
using nnh::detail::SubscriberTable;
using nnh::detail::type_id;

namespace {

struct A {};
struct B {};

SubscriberSlot g_slots[4];

int count_matching(SubscriberTable& t, nnh::detail::TypeId type, std::uint32_t sequence) {
    int n = 0;
    t.for_each_matching(type, sequence, [&](SubscriberSlot&) { ++n; });
    return n;
}

auto noop() {
    return [](const void*) {};
}

} // namespace

NNH_TEST(added_subscribers_match_their_type_only) {
    SubscriberTable t;
    t.reset(g_slots, 4);
    std::uint32_t gen = 0;
    CHECK(t.add(type_id<A>(), noop(), 0, gen) >= 0);
    CHECK(count_matching(t, type_id<A>(), 0) == 1);
    CHECK(count_matching(t, type_id<B>(), 0) == 0);
}

NNH_TEST(removed_subscribers_stop_matching) {
    SubscriberTable t;
    t.reset(g_slots, 4);
    std::uint32_t gen = 0;
    const int slot = t.add(type_id<A>(), noop(), 0, gen);
    CHECK(t.remove(slot, gen));
    CHECK(count_matching(t, type_id<A>(), 0) == 0);
    CHECK(t.active_count() == 0);
}

NNH_TEST(stale_handles_cannot_remove_a_reused_slot) {
    SubscriberTable t;
    t.reset(g_slots, 4);
    std::uint32_t old_gen = 0;
    const int slot = t.add(type_id<A>(), noop(), 0, old_gen);
    CHECK(t.remove(slot, old_gen));
    std::uint32_t new_gen = 0;
    CHECK(t.add(type_id<B>(), noop(), 0, new_gen) == slot); // same slot reused
    CHECK(new_gen != old_gen);
    CHECK(!t.remove(slot, old_gen)); // the old handle is stale
    CHECK(count_matching(t, type_id<B>(), 0) == 1);
}

NNH_TEST(a_full_table_refuses_new_subscribers) {
    SubscriberTable t;
    t.reset(g_slots, 4);
    std::uint32_t gen = 0;
    for (int i = 0; i < 4; ++i) CHECK(t.add(type_id<A>(), noop(), 0, gen) >= 0);
    CHECK(t.add(type_id<A>(), noop(), 0, gen) == -1);
}

NNH_TEST(subscribers_only_see_sequences_from_their_start) {
    SubscriberTable t;
    t.reset(g_slots, 4);
    std::uint32_t gen = 0;
    t.add(type_id<A>(), noop(), 10, gen);
    CHECK(count_matching(t, type_id<A>(), 9) == 0);
    CHECK(count_matching(t, type_id<A>(), 10) == 1);
}

NNH_TEST(removing_during_iteration_skips_the_removed_slot) {
    SubscriberTable t;
    t.reset(g_slots, 4);
    std::uint32_t gen_first = 0;
    std::uint32_t gen_second = 0;
    const int first = t.add(type_id<A>(), noop(), 0, gen_first);
    const int second = t.add(type_id<A>(), noop(), 0, gen_second);
    int visited = 0;
    t.for_each_matching(type_id<A>(), 0, [&](SubscriberSlot&) {
        ++visited;
        t.remove(second, gen_second); // the first handler removes the second one
    });
    CHECK(visited == 1);
    (void)first;
}

NNH_TEST(invalidate_all_deactivates_and_stales_every_handle) {
    SubscriberTable t;
    t.reset(g_slots, 4);
    std::uint32_t gen = 0;
    const int slot = t.add(type_id<A>(), noop(), 0, gen);
    t.invalidate_all();
    CHECK(t.active_count() == 0);
    CHECK(!t.remove(slot, gen));
}
