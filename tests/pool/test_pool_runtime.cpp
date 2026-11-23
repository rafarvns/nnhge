// Run-time pools (spec 020 FR-3, FR-23; T1.5): create() errors, one block of tagged memory, move and moved-from.
// (The behaviour cases of test_pool.cpp and test_pool_iteration.cpp run on this form too, through POOL_CASE.)

#include "nnh/app.hpp"
#include "nnh/memory.hpp"
#include "pool/pool_internal.hpp"
#include "pool_fixture.hpp"

using nnh::ErrorCode;
using nnh::Pool;
using pool_test::Item;

NNH_TEST(pool_create_refuses_capacities_outside_1_to_65535) {
    CHECK(Pool<Item>::create(0).error().code == ErrorCode::InvalidArgument);
    CHECK(Pool<Item>::create(-1).error().code == ErrorCode::InvalidArgument);
    CHECK(Pool<Item>::create(65536).error().code == ErrorCode::InvalidArgument);
    CHECK(Pool<Item>::create(65535).ok());
    CHECK(Pool<Item>::create(1).ok());
}

NNH_TEST(pool_create_reports_out_of_memory) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    nnh::platform_fake::set_region_capacity(nnh::MemRegion::Heap, 1024);
    CHECK(Pool<Item>::create(1000).error().code == ErrorCode::OutOfMemory);
    nnh::platform_fake::reset();
}

NNH_TEST(pool_create_takes_one_block_of_the_pool_tag_or_the_games) {
    nnh::MemTag& pool_tag = nnh::detail::pool_memory_tag();
    const nnh::MemStats before = nnh::mem::tag_stats(pool_tag);
    {
        Pool<Item> pool = pool_test::runtime<Item>(100);
        const nnh::MemStats now = nnh::mem::tag_stats(pool_tag);
        CHECK(now.count == before.count + 1);
        CHECK(now.bytes - before.bytes == nnh::detail::pool_block_bytes(100, sizeof(Item), alignof(Item)));
    }
    CHECK(nnh::mem::tag_stats(pool_tag).count == before.count);
    static NNH_MEM_TAG(bullets);
    {
        auto r = Pool<Item>::create(10, &NNH_MEM_TAG_REF(bullets));
        CHECK(r.ok());
        CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(bullets)).count == 1);
    }
    CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(bullets)).count == 0);
}

NNH_TEST(pool_block_size_follows_the_formula) {
    // PoolCore + 4 B meta per slot + 3 bitmaps + (capacity + 1) objects, each part aligned.
    const std::size_t bytes = nnh::detail::pool_block_bytes(64, 8, 4);
    const std::size_t core = sizeof(nnh::detail::PoolCore);
    CHECK(bytes == core + 64 * 4 + 3 * 2 * 4 + 65 * 8);
}

NNH_TEST(pool_runtime_moves_keep_handles_and_objects) {
    Pool<Item> a = pool_test::runtime<Item>(4);
    auto h = a.acquire(Item{5, 6}).handle;
    Pool<Item> b = static_cast<Pool<Item>&&>(a);
    CHECK(b.get(h) != nullptr && b.get(h)->value == 5);
    CHECK(b.capacity() == 4);
    Pool<Item> c;
    c = static_cast<Pool<Item>&&>(b);
    CHECK(c.alive(h));
}

namespace {

Pool<Item>* g_empty = nullptr;

void acquire_from_an_empty_pool() {
    auto a = g_empty->acquire();
    CHECK(!a.handle && a.object == nullptr);
}
void iterate_an_empty_pool() {
    int n = 0;
    for (auto [h, item] : g_empty->items()) ++n;
    g_empty->each([&](nnh::Handle<Item>, Item&) { ++n; });
    CHECK(n == 0);
}

} // namespace

NNH_TEST(pool_moved_from_or_empty_asserts_and_answers_nothing) {
    Pool<Item> a = pool_test::runtime<Item>(4);
    const auto h = a.acquire().handle;
    Pool<Item> b = static_cast<Pool<Item>&&>(a);
    g_empty = &a;
    CHECK(pool_test::asserts_or_is_ignored(acquire_from_an_empty_pool));
    CHECK(pool_test::asserts_or_is_ignored(iterate_an_empty_pool));
    // Queries that answer "nothing" don't assert: a stale lookup is normal play.
    CHECK(a.get(h) == nullptr && !a.alive(h) && !a.release(h));
    CHECK(a.count() == 0 && a.capacity() == 0 && a.extent() == 0 && a.data() == nullptr);
    Pool<Item> empty;
    CHECK(empty.capacity() == 0 && empty.stats().capacity == 0);
}
