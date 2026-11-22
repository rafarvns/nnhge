// The data index at run time (spec 014 T3.3, FR-13): loaded on demand or by preload() into one block under
// the tag "data" (the file plus a scratch buffer the size of the largest data asset), released by release()
// and by nnh::shutdown() (Debug/Development), refused when damaged, and a registered type the build doesn't
// know warned.

#include <cstring>
#include <string>

#include "data/index_format.hpp"
#include "data_fixture.hpp"
#include "nnh/app.hpp"
#include "nnh/assets.hpp"
#include "nnh/build.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"
#include "test_types.hpp"

namespace fake = nnh::platform_fake;
using nnh::ErrorCode;
namespace fmt = nnh::detail::idxfmt;

namespace {

ErrorCode preload_error() {
    auto r = nnh::data::preload();
    return r ? ErrorCode::None : r.error().code;
}

std::size_t largest(const data_test::Files& f) {
    return data_file::get32(f.index, fmt::kLargestAt);
}

bool last_log_has(const char* text) {
    return std::strstr(fake::state().last_console_line, text) != nullptr;
}

} // namespace

NNH_TEST(preload_keeps_the_index_and_a_scratch_buffer_in_one_block) {
    const data_test::Files f = data_test::standard();
    data_test::start(f);
    CHECK(nnh::data::memory_used() == 0);
    CHECK(nnh::data::preload().ok());
    CHECK(largest(f) == f.goblin.size());
    CHECK(nnh::data::memory_used() == f.index.size() + largest(f));
    CHECK(nnh::mem::live_allocations() > 0);
    data_test::stop();
}

NNH_TEST(the_index_costs_what_the_docs_say) {
    // 32 + 28 per type + 10 per entry + each path + 1, plus the largest data asset.
    const data_test::Files f = data_test::standard();
    std::size_t paths = 0;
    const data_file::IndexSpec spec = data_test::standard_index();
    int entries = 0;
    for (const auto& t : spec.types) {
        for (const auto& p : t.paths)
            paths += p.size() + 1, ++entries;
    }
    for (const auto& group : spec.media) {
        for (const auto& p : group)
            paths += p.size() + 1, ++entries;
    }
    data_test::start(f);
    CHECK(nnh::data::preload().ok());
    CHECK(nnh::data::memory_used() == 32 + 28 * 2 + 10 * entries + paths + largest(f));
    data_test::stop();
}

NNH_TEST(preload_twice_loads_the_index_once) {
    data_test::start();
    CHECK(nnh::data::preload().ok());
    const int live = nnh::mem::live_allocations();
    const int reads = fake::state().asset_reads;
    CHECK(nnh::data::preload().ok());
    CHECK(nnh::mem::live_allocations() == live);
    CHECK(fake::state().asset_reads == reads);
    data_test::stop();
}

NNH_TEST(release_frees_the_index_and_the_next_call_loads_it_again) {
    data_test::start();
    const int before = nnh::mem::live_allocations();
    CHECK(nnh::data::preload().ok());
    nnh::data::release();
    CHECK(nnh::data::memory_used() == 0);
    CHECK(nnh::mem::live_allocations() == before);
    nnh::data::release(); // nothing to free
    CHECK(nnh::data::count<test_data::Enemy>() == 2);
    CHECK(nnh::data::memory_used() > 0);
    data_test::stop();
}

NNH_TEST(shutdown_releases_the_index_before_the_leak_report) {
    if constexpr (nnh::build::kDevToolsEnabled) {
        data_test::start();
        CHECK(nnh::data::preload().ok());
        nnh::shutdown();
        CHECK(nnh::data::memory_used() == 0);
        CHECK(!last_log_has("leak"));
    }
}

NNH_TEST(preload_before_init_is_unsupported) {
    data_test::stop();
    CHECK(preload_error() == ErrorCode::Unsupported);
    CHECK(nnh::data::memory_used() == 0);
}

NNH_TEST(a_game_without_a_data_index_has_no_data) {
    data_test::stop();
    fake::reset();
    static const char kAssets[] = "art/hero.png\n";
    fake::add_asset(".nnh_assets", kAssets, sizeof kAssets - 1);
    CHECK(nnh::init());
    CHECK(preload_error() == ErrorCode::NotFound);
    CHECK(last_log_has("the game has no data assets"));
    CHECK(nnh::data::count<test_data::Enemy>() == 0);
    CHECK(nnh::data::memory_used() == 0);
    data_test::stop();
}

NNH_TEST(a_damaged_index_is_refused_and_nothing_is_kept) {
    data_test::Files f = data_test::standard();
    f.index.back() ^= 1; // CRC mismatch
    data_test::start(f);
    CHECK(preload_error() == ErrorCode::Corrupted);
    CHECK(nnh::data::memory_used() == 0);
    CHECK(last_log_has("CRC"));
    data_test::Files newer = data_test::standard();
    data_file::put16(newer.index, fmt::kVersionAt, 2);
    data_test::start(newer);
    CHECK(preload_error() == ErrorCode::Unsupported);
    CHECK(nnh::data::memory_used() == 0);
    data_test::stop();
}

NNH_TEST(a_registered_type_missing_from_the_index_is_reported) {
    data_test::Files f = data_test::standard();
    data_file::IndexSpec spec = data_test::standard_index();
    spec.types.pop_back(); // no "item": its schema source wasn't in DATA_SCHEMA
    spec.largest = static_cast<std::uint32_t>(f.goblin.size());
    f.index = data_file::index_bytes(spec);
    data_test::start(f);
    CHECK(nnh::data::preload().ok());
    CHECK(last_log_has("type 'item' is not in the build's schema: list its source in DATA_SCHEMA"));
    test_data::Item item;
    auto r = nnh::data::load("items/potion", item);
    CHECK(!r && r.error().code == ErrorCode::NotFound);
    CHECK(nnh::data::count<test_data::Item>() == 0);
    data_test::stop();
}

NNH_TEST(asset_indexes_over_64_kib_are_read) {
    // Spec 014 raises spec 007's cap to 256 KiB (4096 data assets' names).
    std::string list;
    for (int i = 0; i < 3000; ++i)
        list += "enemies/some_long_folder_name/enemy_" + std::to_string(1000 + i) + ".enemy\n";
    CHECK(list.size() > 128 * 1024);
    data_test::stop();
    fake::reset();
    fake::add_asset(".nnh_assets", list.data(), list.size());
    CHECK(nnh::init());
    CHECK(nnh::assets::exists("enemies/some_long_folder_name/enemy_3999.enemy"));
    data_test::stop();
}

NNH_TEST(an_index_shorter_than_its_header_is_corrupted) {
    data_test::Files f = data_test::standard();
    f.index.resize(10);
    data_test::start(f);
    CHECK(preload_error() == ErrorCode::Corrupted);
    CHECK(nnh::data::memory_used() == 0);
    data_test::stop();
}
