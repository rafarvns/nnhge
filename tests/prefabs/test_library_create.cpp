// Creating prefab libraries (spec 021 T2.2, FR-1, FR-15): capacity errors, one block of its tag sized by the layout,
// stamps, moves and moved-from libraries.

#include "prefabs/library_layout.hpp"

#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "prefab_fixture.hpp"

#include <string_view>

using nnh::ErrorCode;
using nnh::PrefabLibrary;
using nnh::PrefabLibraryDesc;
using namespace prefab_test;

namespace {

ErrorCode create_error(const PrefabLibraryDesc& desc) {
    auto r = PrefabLibrary::create(desc);
    return r ? ErrorCode::None : r.error().code;
}

} // namespace

NNH_TEST(library_create_refuses_capacities_out_of_range) {
    CHECK(create_error({.max_prefabs = 0}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.max_prefabs = -1}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.max_prefabs = 1025}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.max_prefabs = 1, .max_entries = -1}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.max_prefabs = 1, .max_entries = 65536}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.max_prefabs = 1, .value_bytes = -1}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.max_prefabs = 1, .value_bytes = (1 << 20) + 1}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.max_prefabs = 1}) == ErrorCode::None);
    CHECK(create_error({.max_prefabs = 1024, .max_entries = 65535, .value_bytes = 1 << 20}) == ErrorCode::None);
}

NNH_TEST(library_create_reports_out_of_memory) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    nnh::platform_fake::set_region_capacity(nnh::MemRegion::Heap, 2048);
    CHECK(create_error({.max_prefabs = 64, .max_entries = 256, .value_bytes = 4096}) == ErrorCode::OutOfMemory);
    nnh::platform_fake::reset();
}

NNH_TEST(library_takes_one_block_of_its_tag_sized_by_the_layout) {
    nnh::MemTag& tag = nnh::detail::prefabs_memory_tag();
    const nnh::MemStats before = nnh::mem::tag_stats(tag);
    const PrefabLibraryDesc desc{.max_prefabs = 9, .max_entries = 20, .value_bytes = 300};
    {
        PrefabLibrary lib = make_library(desc);
        const nnh::MemStats now = nnh::mem::tag_stats(tag);
        CHECK(now.count == before.count + 1);
        CHECK(now.bytes - before.bytes == nnh::detail::library_layout(desc, nnh::build::kDevToolsEnabled).total);
    }
    CHECK(nnh::mem::tag_stats(tag).count == before.count);
    static NNH_MEM_TAG(level_prefabs);
    {
        PrefabLibrary lib = make_library({.max_prefabs = 2, .memory_tag = &NNH_MEM_TAG_REF(level_prefabs)});
        CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(level_prefabs)).count == 1);
    }
    CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(level_prefabs)).count == 0);
}

NNH_TEST(each_library_gets_its_own_stamp) {
    PrefabLibrary a = make_library();
    PrefabLibrary b = make_library();
    const nnh::Prefab pa = must_add(a, "coin", {}, Coin{1});
    const nnh::Prefab pb = must_add(b, "coin", {}, Coin{1});
    CHECK(pa.slot() == 0 && pb.slot() == 0 && pa != pb && pa && pb);
    CHECK(a.valid(pa) && !a.valid(pb) && b.valid(pb) && !b.valid(pa));
}

NNH_TEST(a_moved_library_keeps_its_prefabs_and_the_old_one_is_empty) {
    PrefabLibrary a = make_library();
    const nnh::Prefab coin = must_add(a, "coin", {}, Coin{3});
    PrefabLibrary b(static_cast<PrefabLibrary&&>(a));
    CHECK(b.valid(coin) && b.value<Coin>(coin)->value == 3 && b.count() == 1);
    CHECK(a.count() == 0 && a.capacity() == 0 && !a.valid(coin) && a.stats().capacity == 0);
    CHECK(a.find("coin") == nnh::Prefab{} && a.value<Coin>(coin) == nullptr && std::string_view(a.name(coin)).empty());
    PrefabLibrary c = make_library();
    c = static_cast<PrefabLibrary&&>(b);
    CHECK(c.valid(coin) && b.count() == 0);
}

NNH_TEST(an_empty_library_asserts_on_registration_and_refuses_it) {
    CHECK(asserts_or_is_ignored([] {
        PrefabLibrary empty;
        CHECK(add_error(empty.add("coin", {}, Coin{1})) == ErrorCode::InvalidArgument);
    }));
}
