// Memory of data assets (spec 014 T3.5, FR-13, AC-7): once the index is loaded, loading, listing and
// following references allocate nothing — no engine memory, no operator new — and everything is freed by
// release().

#include "alloc_counter.hpp"
#include "data_fixture.hpp"
#include "nnh/memory.hpp"
#include "test.hpp"
#include "test_types.hpp"

using test_data::Enemy;
using test_data::Item;
namespace data = nnh::data;

NNH_TEST(after_preload_data_calls_allocate_nothing) {
    data_test::start();
    CHECK(data::preload().ok());
    const int live = nnh::mem::live_allocations();
    const std::size_t heap = nnh::mem::region_stats(nnh::MemRegion::Heap).bytes;
    const std::size_t peak = nnh::mem::region_stats(nnh::MemRegion::Heap).peak;
    const std::uint64_t news = alloc_counter::news();
    for (int i = 0; i < 100; ++i) {
        Enemy enemy;
        CHECK(data::load(data::name<Enemy>(i % data::count<Enemy>()), enemy).ok());
        Item item;
        if (enemy.drops.size() > 0) CHECK(data::load(enemy.drops[0].item, item).ok());
        CHECK(data::path(enemy.sprite) != nullptr && data::path(data::ref<Item>(i % 2)) != nullptr);
        CHECK(!data::find<Item>("items/potion").empty());
    }
    CHECK(alloc_counter::news() == news);
    // Errors take no engine memory either (they log a warning, which the fake platform's in-memory log file
    // stores in std:: containers, so operator new isn't checked here).
    for (int i = 0; i < 100; ++i) {
        Enemy enemy;
        CHECK(!data::load("enemies/none", enemy));
    }
    CHECK(nnh::mem::live_allocations() == live);
    CHECK(nnh::mem::region_stats(nnh::MemRegion::Heap).bytes == heap);
    CHECK(nnh::mem::region_stats(nnh::MemRegion::Heap).peak == peak);
    data_test::stop();
}

NNH_TEST(the_data_memory_is_one_allocation_freed_by_release) {
    data_test::start();
    const int live = nnh::mem::live_allocations();
    CHECK(data::count<Item>() == 2); // loads the index on the first call
    CHECK(nnh::mem::live_allocations() == live + 1);
    data::release();
    CHECK(nnh::mem::live_allocations() == live);
    CHECK(data::memory_used() == 0);
    data_test::stop();
}

NNH_TEST(the_counting_operator_new_counts) {
    static int* volatile kept = nullptr; // volatile: the compiler may not drop the pair
    const std::uint64_t before = alloc_counter::news();
    kept = new int(3);
    CHECK(alloc_counter::news() == before + 1);
    delete kept;
}
