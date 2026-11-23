// Pool iteration and raw access (spec 020 FR-7, FR-6; T1.4): ascending slots, releases of self and others during
// items() and each(), acquires during an iteration not visited (also into a hole above the cursor), break, flush
// inside an iteration asserts; data(), extent(), stride(), is_live(), handle_at().

#include "pool_fixture.hpp"

using pool_test::Item;

namespace {

template <typename P>
void fill(P& pool, int n) {
    for (int i = 0; i < n; ++i) pool.acquire().object->value = i;
}

} // namespace

POOL_CASE(pool_items_visit_live_objects_in_ascending_slots, Item, 40) {
    fill(pool, 36);
    pool.release(pool.handle_at(3));
    pool.release(pool.handle_at(33));
    int seen = 0, last = -1;
    bool ordered = true, handles_match = true;
    for (auto [h, item] : pool.items()) {
        ordered = ordered && h.slot() > last;
        handles_match = handles_match && item.value == h.slot() && pool.get(h) == &item;
        last = h.slot();
        ++seen;
    }
    CHECK(seen == 34 && ordered && handles_match);
}

POOL_CASE(pool_each_visits_like_items, Item, 8) {
    fill(pool, 5);
    pool.release(pool.handle_at(2));
    int sum = 0, calls = 0;
    pool.each([&](nnh::Handle<Item> h, Item& item) {
        sum += item.value * 10 + h.slot();
        ++calls;
    });
    CHECK(calls == 4 && sum == (0 + 1 + 3 + 4) * 11);
}

POOL_CASE(pool_release_of_self_and_others_during_iteration_is_safe, Item, 40) {
    fill(pool, 40);
    int visits = 0;
    for (auto [h, item] : pool.items()) {
        ++visits;
        pool.release(h);                                                          // self
        if (h.slot() + 1 < 40) pool.release(pool.handle_at(h.slot() + 1)); // the next one: never visited
    }
    CHECK(visits == 20);
    CHECK(pool.count() == 0);
}

POOL_CASE(pool_acquires_during_an_iteration_are_not_visited, Item, 64) {
    fill(pool, 10);
    pool.release(pool.handle_at(5));
    pool.flush(); // slot 5 is a hole above the cursor's start
    int visits = 0;
    pool.each([&](nnh::Handle<Item>, Item&) {
        ++visits;
        pool.acquire(); // the hole first, then 10, 11, ... in the same and the next words
        pool.acquire();
        pool.acquire();
        pool.acquire();
    });
    CHECK(visits == 9);
    CHECK(pool.count() == 9 + 36);
    int after = 0;
    for (auto [h, item] : pool.items()) ++after; // fresh marks are gone once the iteration ended
    CHECK(after == 45);
}

POOL_CASE(pool_break_out_of_items_ends_the_iteration, Item, 8) {
    fill(pool, 6);
    for (auto [h, item] : pool.items()) {
        if (h.slot() == 2) break;
    }
    pool.release(pool.handle_at(0));
    pool.flush(); // not inside an iteration any more: no assert
    CHECK(pool.acquire().handle.slot() == 0);
}

POOL_CASE(pool_raw_access_reads_the_array_in_place, Item, 8) {
    fill(pool, 4);
    pool.release(pool.handle_at(1));
    CHECK(pool.extent() == 4);
    CHECK(P::stride() == static_cast<int>(sizeof(Item)));
    Item* first = pool.data();
    CHECK(first[2].value == 2 && first[3].value == 3);
    CHECK(first[1].value == 0); // released: T{}
    CHECK(pool.is_live(0) && !pool.is_live(1) && !pool.is_live(7));
    CHECK(pool.handle_at(2) == nnh::detail::make_handle<Item>(2, 1));
    CHECK(!pool.handle_at(1)); // null for a non-live slot
    pool.clear();
    CHECK(pool.extent() == 0);
}

namespace {

nnh::Pool<Item, 4>* g_iter_pool = nullptr;

void flush_inside_each() {
    g_iter_pool->each([](nnh::Handle<Item>, Item&) { g_iter_pool->flush(); });
}

} // namespace

NNH_TEST(pool_flush_inside_an_iteration_asserts_or_is_ignored) {
    nnh::Pool<Item, 4> pool;
    auto a = pool.acquire();
    pool.acquire();
    pool.release(a.handle);
    g_iter_pool = &pool;
    CHECK(pool_test::asserts_or_is_ignored(flush_inside_each));
    pool_test::recover(pool);
    CHECK(pool.acquire().handle.slot() == 2); // nothing was flushed
    pool.flush();
    CHECK(pool.acquire().handle.slot() == 0);
}
