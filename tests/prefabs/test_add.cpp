// Registering prefabs (spec 021 T2.4, FR-2, FR-3, FR-15): entries in order, tags, placement and hook stored, values
// copied once; every refusal checked before anything is written (the block's hash is unchanged after it).
//
// Compile-time refusals (each would stop the build; kept here as documentation, checked by hand):
//   struct Pinned { Pinned(const Pinned&) = delete; };  lib.add("p", {}, Pinned{});    // not copy-constructible
//   struct alignas(32) Wide { int v; };                  lib.add("w", {}, Wide{});      // aligned over 16 bytes
//   struct Tile { int x, y; };                           nnh::place(&Tile::x);          // not a Vec2 or Rect member
//   lib.add("n", {}, 5);                                                                // not a class type

#include "prefab_fixture.hpp"

#include <utility>

using nnh::ComponentValue;
using nnh::ErrorCode;
using nnh::Prefab;
using nnh::PrefabLibrary;
using namespace prefab_test;

namespace {

template <int... N>
nnh::Result<Prefab> add_kinds(PrefabLibrary& lib, const char* name, std::integer_sequence<int, N...>) {
    return lib.add(name, {}, K<N>{}...);
}

} // namespace

NNH_TEST(add_stores_entries_in_order_with_tags_placement_and_hook) {
    PrefabLibrary lib = make_library();
    int calls = 0;
    int* counter = &calls;
    const Prefab p = must_add(lib, "coin",
                              {.tags = {kCoinTag, kFx},
                               .place = nnh::place(&Body::box),
                               .on_spawn = [counter](nnh::World&, nnh::Entity, const nnh::SpawnInfo&) { ++*counter; }},
                              Coin{7}, Body{{1, 2, 3, 4}, 9});
    nnh::KindId kinds[4] = {};
    CHECK(lib.kinds(p, kinds, 4) == 2 && kinds[0] == nnh::kind_id<Coin>() && kinds[1] == nnh::kind_id<Body>());
    nnh::Tag tags[8] = {};
    CHECK(lib.tags(p, tags, 8) == 2 && tags[0] == kCoinTag && tags[1] == kFx);
    CHECK(lib.value<Coin>(p)->value == 7 && lib.value<Body>(p)->box.w == 3 && lib.value<Body>(p)->layers == 9);
    const nnh::detail::PrefabRecord& r = nnh::PrefabAccess::block(lib)->records[p.slot()];
    CHECK(r.place_entry == 1 && r.place_offset == 0 && r.place_flags == nnh::detail::kPlaceRect && r.hooks == 1);
    CHECK(static_cast<bool>(r.hook) && r.hook_base == nnh::detail::kNoHookBase);
}

NNH_TEST(a_prefab_with_only_a_hook_or_nothing_is_valid) {
    PrefabLibrary lib = make_library();
    const Prefab hook = must_add(lib, "burst", {.on_spawn = [](nnh::World&, nnh::Entity, const nnh::SpawnInfo&) {}});
    const Prefab bare = must_add(lib, "marker", {.tags = {kFx}});
    CHECK(lib.kinds(hook, nullptr, 0) == 0 && lib.kinds(bare, nullptr, 0) == 0 && lib.tags(bare, nullptr, 0) == 1);
}

NNH_TEST(add_copies_each_value_once_and_the_library_destroys_them) {
    Counted::reset();
    {
        PrefabLibrary lib = make_library();
        Counted c;
        c.id = 5;
        must_add(lib, "counted", {}, c, Coin{1});
        CHECK(Counted::alive() == 2); // c and its copy in the block
        CHECK(lib.value<Counted>(lib.find("counted"))->id == 5);
    }
    CHECK(Counted::alive() == 0);
}

NNH_TEST(add_refuses_a_bad_name) {
    PrefabLibrary lib = make_library();
    const std::uint32_t before = block_hash(lib);
    CHECK(add_error(lib.add(nullptr, {}, Coin{1})) == ErrorCode::InvalidArgument);
    CHECK(add_error(lib.add("", {}, Coin{1})) == ErrorCode::InvalidArgument);
    CHECK(block_hash(lib) == before);
}

NNH_TEST(add_refuses_a_kind_twice_and_33_entries) {
    PrefabLibrary lib = make_library({.max_prefabs = 4, .max_entries = 128, .value_bytes = 1024});
    const std::uint32_t before = block_hash(lib);
    CHECK(add_error(lib.add("twice", {}, Coin{1}, Body{}, Coin{2})) == ErrorCode::InvalidArgument);
    CHECK(add_error(add_kinds(lib, "k33", std::make_integer_sequence<int, 33>{})) == ErrorCode::LimitReached);
    CHECK(block_hash(lib) == before);
    CHECK(add_kinds(lib, "k32", std::make_integer_sequence<int, 32>{}).ok());
}

NNH_TEST(add_refuses_no_tag_before_another_tag) {
    PrefabLibrary lib = make_library();
    const std::uint32_t before = block_hash(lib);
    CHECK(add_error(lib.add("gap", {.tags = {kFx, nnh::kNoTag, kEnemy}}, Coin{1})) == ErrorCode::InvalidArgument);
    CHECK(block_hash(lib) == before);
    CHECK(lib.add("dup", {.tags = {kFx, kFx, kEnemy}}, Coin{1}).ok());
    CHECK(lib.tags(lib.find("dup"), nullptr, 0) == 2); // duplicates dropped
}

NNH_TEST(add_refuses_a_placement_kind_not_among_the_entries_or_past_its_end) {
    PrefabLibrary lib = make_library();
    const std::uint32_t before = block_hash(lib);
    CHECK(add_error(lib.add("p", {.place = nnh::place(&Mover::at)}, Coin{1})) == ErrorCode::InvalidArgument);
    const nnh::Placement past{.kind = nnh::kind_id<Coin>(), .offset = 0, .rect = false};
    CHECK(add_error(lib.add("q", {.place = past}, Coin{1})) == ErrorCode::InvalidArgument); // a Vec2 in 4 bytes
    const nnh::Placement rect{.kind = nnh::kind_id<Mover>(), .offset = 4, .rect = true};
    CHECK(add_error(lib.add("r", {.place = rect}, Mover{})) == ErrorCode::InvalidArgument); // 4 + 16 > 16
    CHECK(block_hash(lib) == before);
    CHECK(lib.add("s", {.place = nnh::place(&Mover::v)}, Mover{}).ok());
}

NNH_TEST(add_refuses_full_capacities_and_changes_nothing) {
    PrefabLibrary lib = make_library({.max_prefabs = 2, .max_entries = 3, .value_bytes = 72});
    must_add(lib, "big", {}, Big{}); // 64 bytes at offset 0
    std::uint32_t before = block_hash(lib);
    // value bytes with alignment: a Body (20 B, align 4) would end at 84 > 72.
    CHECK(add_error(lib.add("body", {}, Body{})) == ErrorCode::LimitReached);
    CHECK(add_error(lib.add("three", {}, Coin{}, Bob{}, Mover{})) == ErrorCode::LimitReached); // 4 entries > 3
    CHECK(block_hash(lib) == before);
    must_add(lib, "coins", {}, Coin{}, Bob{}); // 64 + 4 + 4 = 72: fits exactly
    before = block_hash(lib);
    CHECK(add_error(lib.add("one_more", {.tags = {kFx}})) == ErrorCode::LimitReached); // 2 prefabs
    CHECK(block_hash(lib) == before && lib.count() == 2);
}

NNH_TEST(add_refuses_one_entry_or_a_few_bytes_past_a_capacity) {
    // Mutation testing (plan 021 T8.1): the limits are exact.
    PrefabLibrary entries = make_library({.max_prefabs = 4, .max_entries = 2, .value_bytes = 1024});
    must_add(entries, "two", {}, Coin{}, Bob{});
    CHECK(add_error(entries.add("one_more", {}, Coin{})) == ErrorCode::LimitReached);
    PrefabLibrary bytes = make_library({.max_prefabs = 4, .max_entries = 8, .value_bytes = 8});
    must_add(bytes, "a", {}, Coin{});
    must_add(bytes, "b", {}, Bob{});
    CHECK(add_error(bytes.add("c", {}, K<0>{})) == ErrorCode::LimitReached); // 12 bytes > 8
}

NNH_TEST(add_refuses_a_value_with_a_bad_alignment) {
    PrefabLibrary lib = make_library();
    const Coin coin{1};
    ComponentValue v = nnh::component_value(coin);
    v.align = 3;
    CHECK(add_error(lib.add("three", {}, &v, 1)) == ErrorCode::InvalidArgument);
    v.align = 32;
    CHECK(add_error(lib.add("wide", {}, &v, 1)) == ErrorCode::InvalidArgument);
    v.align = 4;
    CHECK(lib.add("ok", {}, &v, 1).ok());
}

NNH_TEST(add_aligns_each_value_to_its_type) {
    struct alignas(16) Aligned {
        float v[4] = {1, 2, 3, 4};
    };
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "a", {}, Coin{1}, Aligned{});
    const auto* value = lib.value<Aligned>(p);
    CHECK(reinterpret_cast<std::uintptr_t>(value) % 16 == 0 && value->v[3] == 4);
    CHECK(lib.stats().value_bytes == 32); // 4, padding to 16, 16
}

NNH_TEST(the_array_form_registers_component_values) {
    PrefabLibrary lib = make_library();
    const Coin coin{4};
    const Body body{{0, 0, 8, 8}, 2};
    const ComponentValue values[] = {nnh::component_value(coin), nnh::component_value(body)};
    const auto r = lib.add("coin", {.tags = {kCoinTag}}, values, 2);
    CHECK(r.ok() && lib.value<Coin>(r.value())->value == 4 && lib.value<Body>(r.value())->box.w == 8);
    CHECK(add_error(lib.add("none", {}, static_cast<const ComponentValue*>(nullptr), 1)) == ErrorCode::InvalidArgument);
    CHECK(add_error(lib.add("negative", {}, values, -1)) == ErrorCode::InvalidArgument);
    const ComponentValue broken[] = {{.id = nnh::kind_id<Coin>(), .value = &coin, .size = 4, .align = 3}};
    CHECK(add_error(lib.add("broken", {}, broken, 1)) == ErrorCode::InvalidArgument);
}

// Regression (spec 022 stage 9, Linux): with `children[...] = {}` in PrefabOptions, GCC 13 and 14 refused every braced
// `{.tags = ...}` options argument of the variadic add() ("cannot convert ... to const PrefabOptions&").
template <typename T>
concept AddsWithBracedOptions = requires(PrefabLibrary& lib, nnh::Tag tag, const T& value) {
    lib.add("x", {.tags = {tag}}, value);
    lib.add("x", {.tags = {tag}, .children = {nnh::child(Prefab{})}}, value);
};
static_assert(AddsWithBracedOptions<Coin>, "PrefabLibrary::add must accept braced options on every compiler");

NNH_TEST(braced_options_leave_unnamed_children_null) {
    const nnh::PrefabOptions options{.tags = {kCoinTag}};
    nnh::PrefabOptions plain; // default-initialised: each member's own default
    CHECK(!options.children[0].prefab && options.children[7].tree == nullptr && !options.children[3].has_local);
    CHECK(!plain.children[0].prefab && plain.children[7].tree == nullptr && plain.children[5].local.scale.x == 1);
}
