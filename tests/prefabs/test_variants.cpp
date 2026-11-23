// Variants (spec 021 T3.1, T3.2, FR-4, FR-11): flattened at registration — entries replaced in place or appended,
// inherited values shared with the base (no copy, destroyed once), tags united, placement inherited or replaced, hook
// chains of up to 8. (The chain's order when hooks run is checked in test_spawn_hooks.cpp.)

#include "prefab_fixture.hpp"

using nnh::ErrorCode;
using nnh::Prefab;
using nnh::PrefabLibrary;
using namespace prefab_test;

namespace {

const nnh::detail::PrefabRecord& record(const PrefabLibrary& lib, Prefab p) {
    return nnh::PrefabAccess::block(lib)->records[p.slot()];
}

void no_op(nnh::World&, nnh::Entity, const nnh::SpawnInfo&) {}

} // namespace

// ---- Part 1: entries, values, tags, placement, bases ----

NNH_TEST(a_variant_replaces_its_bases_entry_of_the_same_kind_in_place) {
    PrefabLibrary lib = make_library();
    const Prefab walker = must_add(lib, "walker", {}, Body{{0, 0, 8, 8}, 1}, Coin{1}, Bob{2});
    const Prefab rich = must_add(lib, "rich_walker", {.base = walker}, Coin{5});
    nnh::KindId kinds[4] = {};
    CHECK(lib.kinds(rich, kinds, 4) == 3);
    CHECK(kinds[0] == nnh::kind_id<Body>() && kinds[1] == nnh::kind_id<Coin>() && kinds[2] == nnh::kind_id<Bob>());
    CHECK(lib.value<Coin>(rich)->value == 5 && lib.value<Coin>(walker)->value == 1);
    CHECK(lib.value<Body>(rich) == lib.value<Body>(walker)); // shared: the same stored value
    CHECK(lib.stats().value_bytes == 20 + 4 + 4 + 4 && lib.stats().entries == 6);
}

NNH_TEST(a_variant_appends_kinds_its_base_lacks_in_call_order) {
    PrefabLibrary lib = make_library();
    const Prefab walker = must_add(lib, "walker", {}, Body{}, Coin{1});
    const Prefab flyer = must_add(lib, "flyer", {.base = walker}, Mover{{1, 2}, {}}, Coin{3}, Bob{4});
    nnh::KindId kinds[8] = {};
    CHECK(lib.kinds(flyer, kinds, 8) == 4);
    CHECK(kinds[0] == nnh::kind_id<Body>() && kinds[1] == nnh::kind_id<Coin>());
    CHECK(kinds[2] == nnh::kind_id<Mover>() && kinds[3] == nnh::kind_id<Bob>());
    CHECK(lib.value<Coin>(flyer)->value == 3 && lib.value<Mover>(flyer)->at.y == 2);
}

NNH_TEST(inherited_values_are_shared_not_copied_and_destroyed_once) {
    Counted::reset();
    {
        PrefabLibrary lib = make_library();
        Counted c;
        c.id = 7;
        const Prefab base = must_add(lib, "base", {}, c, Coin{1});
        CHECK(Counted::alive() == 2);
        const Prefab plain = must_add(lib, "plain", {.base = base}, Coin{2});
        CHECK(Counted::alive() == 2); // no copy for the inherited Counted
        Counted d;
        d.id = 8;
        must_add(lib, "replaced", {.base = plain}, d);
        CHECK(Counted::alive() == 4); // c, d and two stored copies
        CHECK(lib.value<Counted>(lib.find("replaced"))->id == 8 && lib.value<Counted>(plain)->id == 7);
    }
    CHECK(Counted::alive() == 0); // each stored value destroyed once
}

NNH_TEST(a_variant_unites_its_tags_with_its_bases) {
    PrefabLibrary lib = make_library();
    const Prefab base = must_add(lib, "base", {.tags = {kEnemy, kFx}}, Coin{1});
    const Prefab v = must_add(lib, "v", {.base = base, .tags = {kFx, kFlying}});
    nnh::Tag tags[8] = {};
    CHECK(lib.tags(v, tags, 8) == 3 && tags[0] == kEnemy && tags[1] == kFx && tags[2] == kFlying);
    const nnh::Tag six[] = {nnh::tag("t1"), nnh::tag("t2"), nnh::tag("t3"), nnh::tag("t4"), nnh::tag("t5"),
                            nnh::tag("t6")};
    const Prefab many = must_add(lib, "many", {.tags = {six[0], six[1], six[2], six[3], six[4], six[5]}});
    CHECK(lib.add("eight", {.base = many, .tags = {six[0], kEnemy, kFx}}).ok());
    const std::uint32_t before = block_hash(lib);
    CHECK(add_error(lib.add("nine", {.base = many, .tags = {kEnemy, kFx, kFlying}})) == ErrorCode::LimitReached);
    CHECK(block_hash(lib) == before);
}

NNH_TEST(a_variant_inherits_its_bases_placement_or_replaces_it) {
    PrefabLibrary lib = make_library();
    const Prefab base = must_add(lib, "base", {.place = nnh::place(&Body::box, true)}, Body{}, Mover{});
    const Prefab same = must_add(lib, "same", {.base = base}, Coin{1});
    const Prefab moved = must_add(lib, "moved", {.base = base, .place = nnh::place(&Mover::v)}, Coin{1});
    const auto& a = record(lib, same);
    CHECK(a.place_entry == 0 && a.place_flags == (nnh::detail::kPlaceRect | nnh::detail::kPlaceKeepSize));
    const auto& b = record(lib, moved);
    CHECK(b.place_entry == 1 && b.place_offset == 8 && b.place_flags == 0);
    CHECK(add_error(lib.add("lost", {.base = base, .place = nnh::place(&Mover::at)}, Coin{1})) == ErrorCode::None);
}

NNH_TEST(a_variant_needs_a_base_of_its_own_library) {
    PrefabLibrary lib = make_library();
    PrefabLibrary other = make_library();
    must_add(lib, "base", {}, Coin{1});
    const Prefab foreign = must_add(other, "foreign", {}, Coin{1});
    const std::uint32_t before = block_hash(lib);
    CHECK(add_error(lib.add("v", {.base = foreign}, Coin{2})) == ErrorCode::InvalidArgument);
    CHECK(add_error(lib.add("w", {.base = Prefab::from_bits(0x00000007u)}, Coin{2})) == ErrorCode::InvalidArgument);
    CHECK(block_hash(lib) == before);
}

NNH_TEST(a_variant_past_32_entries_is_refused) {
    PrefabLibrary lib = make_library({.max_prefabs = 4, .max_entries = 128, .value_bytes = 1024});
    const auto r = lib.add("base", {}, K<0>{}, K<1>{}, K<2>{}, K<3>{}, K<4>{}, K<5>{}, K<6>{}, K<7>{}, K<8>{}, K<9>{},
                           K<10>{}, K<11>{}, K<12>{}, K<13>{}, K<14>{}, K<15>{}, K<16>{}, K<17>{}, K<18>{}, K<19>{},
                           K<20>{}, K<21>{}, K<22>{}, K<23>{}, K<24>{}, K<25>{}, K<26>{}, K<27>{}, K<28>{}, K<29>{},
                           K<30>{});
    CHECK(r.ok());
    CHECK(lib.add("v32", {.base = r.value()}, K<31>{}, K<0>{}).ok()); // one replaced, one appended: 32
    CHECK(add_error(lib.add("v33", {.base = r.value()}, K<31>{}, K<32>{})) == ErrorCode::LimitReached);
}

// ---- Part 2: hook chains and queries ----

NNH_TEST(variant_hook_chains_link_to_the_nearest_hooked_base) {
    PrefabLibrary lib = make_library();
    const Prefab a = must_add(lib, "a", {.on_spawn = no_op}, Coin{1});
    const Prefab b = must_add(lib, "b", {.base = a}, Bob{});              // no hook of its own
    const Prefab c = must_add(lib, "c", {.base = b, .on_spawn = no_op}); // a's then c's
    const Prefab d = must_add(lib, "d", {.base = c});
    CHECK(record(lib, a).hooks == 1 && record(lib, a).hook_base == nnh::detail::kNoHookBase);
    CHECK(record(lib, b).hooks == 1 && record(lib, b).hook_base == a.slot() && !record(lib, b).hook);
    CHECK(record(lib, c).hooks == 2 && record(lib, c).hook_base == a.slot() && record(lib, c).hook);
    CHECK(record(lib, d).hooks == 2 && record(lib, d).hook_base == c.slot());
    nnh::KindId kinds[4] = {};
    CHECK(lib.kinds(d, kinds, 4) == 2 && lib.value<Coin>(d)->value == 1 && lib.has<Bob>(d));
}

NNH_TEST(a_chain_of_eight_hooks_is_accepted_and_a_ninth_refused) {
    PrefabLibrary lib = make_library();
    Prefab p = must_add(lib, "h1", {.on_spawn = no_op}, Coin{1});
    const char* names[] = {"h2", "h3", "h4", "h5", "h6", "h7", "h8"};
    for (const char* n : names) p = must_add(lib, n, {.base = p, .on_spawn = no_op});
    CHECK(record(lib, p).hooks == 8);
    CHECK(lib.add("plain9", {.base = p}).ok()); // no hook of its own: still 8
    const std::uint32_t before = block_hash(lib);
    CHECK(add_error(lib.add("h9", {.base = p, .on_spawn = no_op})) == ErrorCode::LimitReached);
    CHECK(block_hash(lib) == before);
}

NNH_TEST(a_variant_of_a_variant_of_a_variant_flattens_every_level) {
    PrefabLibrary lib = make_library();
    const Prefab a = must_add(lib, "a", {.tags = {kEnemy}}, Body{{0, 0, 4, 4}, 1}, Coin{1});
    const Prefab b = must_add(lib, "b", {.base = a, .tags = {kFlying}}, Bob{2});
    const Prefab c = must_add(lib, "c", {.base = b}, Coin{3}, Mover{});
    const Prefab d = must_add(lib, "d", {.base = c, .tags = {kFx}}, Bob{4});
    nnh::KindId kinds[8] = {};
    CHECK(lib.kinds(d, kinds, 8) == 4);
    CHECK(kinds[0] == nnh::kind_id<Body>() && kinds[1] == nnh::kind_id<Coin>() && kinds[2] == nnh::kind_id<Bob>() &&
          kinds[3] == nnh::kind_id<Mover>());
    CHECK(lib.value<Coin>(d)->value == 3 && lib.value<Bob>(d)->phase == 4 && lib.value<Body>(d) == lib.value<Body>(a));
    nnh::Tag tags[8] = {};
    CHECK(lib.tags(d, tags, 8) == 3 && tags[0] == kEnemy && tags[1] == kFlying && tags[2] == kFx);
}
