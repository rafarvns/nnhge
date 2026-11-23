// Pools (spec 020 FR-2..FR-6, FR-8, FR-23; T1.3): lowest free slot, full → null + miss, release → T{} +
// quarantine, flush, clear, stale and forged handles, the checked accessor, constructions balance, no allocation.
// Every case runs on Pool<T, N> and on Pool<T>::create(N).

#include "alloc_counter.hpp"
#include "nnh/memory.hpp"
#include "pool_fixture.hpp"

using pool_test::Counted;
using pool_test::Item;

POOL_CASE(pool_acquires_the_lowest_free_slot, Item, 8) {
    for (int i = 0; i < 3; ++i) {
        auto [h, item] = pool.acquire();
        CHECK(h.slot() == i);
        CHECK(h.generation() == 1);
        CHECK(item != nullptr && item->value == 0 && item->other == 7); // a T{} waited in the slot
    }
    CHECK(pool.count() == 3);
    CHECK(pool.capacity() == 8);
}

POOL_CASE(pool_acquire_with_a_value_copies_or_moves_it, Item, 4) {
    const Item seven{7, 8};
    auto a = pool.acquire(seven);
    CHECK(a.object->value == 7 && a.object->other == 8);
    auto b = pool.acquire(Item{9, 10});
    CHECK(b.object->value == 9 && pool.get(b.handle)->other == 10);
}

POOL_CASE(pool_full_returns_null_and_counts_a_miss_without_asserting, Item, 2) {
    pool.acquire();
    pool.acquire();
    auto [h, item] = pool.acquire();
    CHECK(!h && item == nullptr);
    CHECK(pool.stats().misses == 1);
    CHECK(pool.count() == 2);
}

POOL_CASE(pool_release_resets_the_slot_and_makes_the_handle_stale, Item, 4) {
    auto [h, item] = pool.acquire();
    item->value = 42;
    CHECK(pool.release(h));
    CHECK(!pool.alive(h));
    CHECK(pool.get(h) == nullptr);
    CHECK(item->value == 0 && item->other == 7); // the old pointer reads a T{}, never freed memory
    CHECK(pool.count() == 0);
}

POOL_CASE(pool_released_slot_is_quarantined_until_the_flush, Item, 4) {
    auto a = pool.acquire();
    pool.acquire();
    pool.release(a.handle);
    CHECK(pool.acquire().handle.slot() == 2); // not slot 0: same frame
    pool.flush();
    auto again = pool.acquire();
    CHECK(again.handle.slot() == 0);
    CHECK(again.handle.generation() == 2);
    CHECK(again.handle != a.handle);
}

POOL_CASE(pool_double_release_and_null_release_return_false, Item, 4) {
    auto a = pool.acquire();
    CHECK(pool.release(a.handle));
    CHECK(!pool.release(a.handle));
    CHECK(!pool.release(nnh::Handle<Item>{}));
    CHECK(pool.count() == 0);
}

POOL_CASE(pool_refuses_a_forged_next_generation_handle, Item, 4) {
    auto a = pool.acquire();
    pool.release(a.handle);
    pool.flush();
    // The slot is free with generation 2: a handle naming it must not read as live.
    const auto forged = nnh::detail::make_handle<Item>(0, 2);
    CHECK(!pool.alive(forged));
    CHECK(pool.get(forged) == nullptr);
    CHECK(!pool.release(forged));
    const auto out_of_range = nnh::detail::make_handle<Item>(60000, 1);
    CHECK(!pool.alive(out_of_range) && pool.get(out_of_range) == nullptr);
}

POOL_CASE(pool_clear_restarts_at_the_first_load_slots, Item, 8) {
    for (int i = 0; i < 5; ++i) pool.acquire();
    pool.release(pool.handle_at(1));
    pool.clear();
    CHECK(pool.count() == 0 && pool.extent() == 0);
    for (int i = 0; i < 5; ++i) CHECK(pool.acquire().handle.slot() == i);
}

POOL_CASE(pool_stats_count_live_peak_and_misses_until_clear, Item, 2) {
    auto a = pool.acquire();
    pool.acquire();
    pool.acquire(); // miss
    pool.release(a.handle);
    const nnh::PoolStats s = pool.stats();
    CHECK(s.live == 1 && s.peak == 2 && s.capacity == 2 && s.misses == 1);
    pool.clear();
    const nnh::PoolStats c = pool.stats();
    CHECK(c.live == 0 && c.peak == 0 && c.misses == 0 && c.capacity == 2);
}

POOL_CASE(pool_generation_wraps_after_65535_reuses_of_a_slot, Item, 1) {
    // The documented limit (FR-2): a handle kept across 65 535 reuses of its slot reads as live again.
    const auto first = pool.acquire().handle;
    nnh::Handle<Item> h = first;
    for (int i = 0; i < 65535; ++i) {
        pool.release(h);
        pool.flush();
        h = pool.acquire().handle;
        if (i == 65533) CHECK(h.generation() == 65535);
    }
    CHECK(h.generation() == 1);
    CHECK(pool.alive(first)); // pinned on purpose: a change in the wrap rule shows here
}

POOL_CASE(pool_acquire_and_release_allocate_nothing, Item, 16) {
    const auto before = alloc_counter::news();
    const int engine_before = nnh::mem::live_allocations();
    for (int i = 0; i < 1000; ++i) {
        auto a = pool.acquire();
        pool.release(a.handle);
        pool.flush();
    }
    CHECK(alloc_counter::news() == before);
    CHECK(nnh::mem::live_allocations() == engine_before);
}

NNH_TEST(pool_inline_form_allocates_nothing_at_all) {
    const int before = nnh::mem::live_allocations();
    const auto news = alloc_counter::news();
    {
        nnh::Pool<Item, 32> pool;
        pool.acquire();
        CHECK(nnh::mem::live_allocations() == before);
    }
    CHECK(alloc_counter::news() == news);
}

NNH_TEST(pool_constructions_and_destructions_balance) {
    Counted::reset();
    {
        nnh::Pool<Counted, 4> pool;
        CHECK(Counted::alive() == 5); // four slots and the scratch hold a T{}
        auto a = pool.acquire(Counted{});
        pool.acquire();
        pool.release(a.handle);
        pool.clear();
    }
    CHECK(Counted::alive() == 0);
    Counted::reset();
    {
        nnh::Pool<Counted> pool = pool_test::runtime<Counted>(4);
        auto a = pool.acquire();
        a.object->id = 3;
        pool.release(a.handle);
    }
    CHECK(Counted::alive() == 0);
}

NNH_TEST(pool_value_constructions_keep_the_value) {
    nnh::Pool<Counted, 2> pool;
    Counted c;
    c.id = 11;
    auto a = pool.acquire(c);
    CHECK(a.object->id == 11 && c.id == 11);
    auto b = pool.acquire(static_cast<Counted&&>(c));
    CHECK(b.object->id == 11 && c.id == -1);
}

namespace {

nnh::Pool<Item, 4>* g_pool = nullptr;
nnh::Handle<Item> g_stale{};

void at_on_a_stale_handle() {
    Item& item = g_pool->at(g_stale);
    item.value = 99; // Release: the scratch T{}, never a live object
}

} // namespace

NNH_TEST(pool_checked_accessor_asserts_or_returns_a_scratch_value) {
    nnh::Pool<Item, 4> pool;
    auto a = pool.acquire();
    a.object->value = 5;
    CHECK(pool.at(a.handle).value == 5);
    auto b = pool.acquire();
    pool.release(b.handle);
    g_pool = &pool;
    g_stale = b.handle;
    CHECK(pool_test::asserts_or_is_ignored(at_on_a_stale_handle));
    CHECK(a.object->value == 5); // no live object was handed out
    if (!nnh::build::kAssertsEnabled) CHECK(pool.at(g_stale).value == 0); // re-made T{} each time
}
