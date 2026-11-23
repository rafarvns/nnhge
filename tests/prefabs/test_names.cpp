// Prefab names (spec 021 T2.3, FR-3, FR-14): the FNV-1a hash at compile time and in one pass at run time, the
// open-addressing table (collisions, wrap-around, a full library), find by all three forms, duplicates refused.

#include "prefab_fixture.hpp"

#include <cstdio>
#include <cstring>
#include <string_view>

using nnh::ErrorCode;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::prefab_name;
using nnh::detail::hash_string;
using namespace prefab_test;

namespace {

void name_of(char* out, int i) { std::snprintf(out, 16, "enemy_%03d", i); }

/// Two names of the form "w<n>" whose hashes land on the same slot of a table of `mask` + 1 slots, at slot `slot`.
void colliding_names(std::uint32_t mask, std::uint32_t slot, char (&a)[16], char (&b)[16]) {
    int found = 0;
    for (int i = 0; found < 2 && i < 100000; ++i) {
        char name[16];
        std::snprintf(name, sizeof name, "w%d", i);
        if ((hash_string(name) & mask) != slot) continue;
        std::memcpy(found == 0 ? a : b, name, sizeof name);
        ++found;
    }
    CHECK(found == 2);
}

} // namespace

NNH_TEST(prefab_names_hash_the_same_at_compile_time_and_at_run_time) {
    constexpr nnh::PrefabName coin = prefab_name("coin");
    static_assert(static_cast<std::uint32_t>(coin) == nnh::detail::fnv1a32("coin"));
    const char buffer[] = {'c', 'o', 'i', 'n', 'x'};
    CHECK(prefab_name(buffer, 4) == coin);
    CHECK(hash_string("coin") == static_cast<std::uint32_t>(coin));
    // FNV-1a 32 test vectors.
    CHECK(hash_string("") == 0x811C9DC5u && hash_string("a") == 0xE40C292Cu && hash_string("foobar") == 0xBF9CF968u);
    for (int i = 0; i < 1000; ++i) {
        char name[16];
        name_of(name, i);
        CHECK(hash_string(name) == nnh::detail::fnv1a32(name));
    }
}

NNH_TEST(find_by_literal_string_and_buffer_gives_the_same_prefab) {
    PrefabLibrary lib = make_library();
    const Prefab coin = must_add(lib, "coin", {}, Coin{1});
    const Prefab walker = must_add(lib, "walker", {}, Body{});
    const char type[] = "walker!!";
    CHECK(lib.find(prefab_name("coin")) == coin && lib.find("coin") == coin);
    CHECK(lib.find(type, 6) == walker && lib.find("walker") == walker);
    CHECK(lib.find("coinn") == Prefab{} && lib.find(prefab_name("door")) == Prefab{} && lib.find("") == Prefab{});
    CHECK(lib.find(nullptr) == Prefab{});
}

NNH_TEST(a_name_registered_twice_or_a_colliding_hash_is_refused) {
    PrefabLibrary lib = make_library();
    must_add(lib, "costarring", {}, Coin{1});
    const std::uint32_t before = block_hash(lib);
    CHECK(add_error(lib.add("costarring", {}, Coin{2})) == ErrorCode::InvalidArgument);
    // "liquid" and "costarring" have the same FNV-1a 32 hash: a collision is always caught at registration.
    static_assert(nnh::detail::fnv1a32("liquid") == nnh::detail::fnv1a32("costarring"));
    CHECK(add_error(lib.add("liquid", {}, Coin{2})) == ErrorCode::InvalidArgument);
    CHECK(block_hash(lib) == before && lib.count() == 1);
}

NNH_TEST(names_on_the_same_table_slot_are_both_found) {
    // 4 prefabs: a table of 8 slots. Two names on slot 3, then two on the last slot (the second wraps to slot 0).
    PrefabLibrary lib = make_library({.max_prefabs = 4, .max_entries = 8, .value_bytes = 64});
    char a[16], b[16], c[16], d[16];
    colliding_names(7, 3, a, b);
    colliding_names(7, 7, c, d);
    const Prefab pa = must_add(lib, a, {}, Coin{1});
    const Prefab pb = must_add(lib, b, {}, Coin{2});
    const Prefab pc = must_add(lib, c, {}, Coin{3});
    const Prefab pd = must_add(lib, d, {}, Coin{4});
    CHECK(lib.find(a) == pa && lib.find(b) == pb && lib.find(c) == pc && lib.find(d) == pd);
    const nnh::detail::PrefabBlock* block = nnh::PrefabAccess::block(lib);
    CHECK(block->table[3] == 1 && block->table[4] == 2 && block->table[7] == 3 && block->table[0] == 4);
    CHECK(lib.find("not here") == Prefab{});
}

NNH_TEST(a_full_library_finds_every_name_and_refuses_one_more) {
    PrefabLibrary lib = make_library({.max_prefabs = 128, .max_entries = 128, .value_bytes = 128 * 4});
    for (int i = 0; i < 128; ++i) {
        char name[16];
        name_of(name, i);
        CHECK(must_add(lib, name, {}, Coin{i}).slot() == i);
    }
    for (int i = 0; i < 128; ++i) {
        char name[16];
        name_of(name, i);
        CHECK(lib.find(name).slot() == i && lib.value<Coin>(lib.find(name))->value == i);
    }
    CHECK(add_error(lib.add("enemy_128", {}, Coin{0})) == ErrorCode::LimitReached);
    CHECK(lib.find("enemy_128") == Prefab{} && lib.count() == 128);
}

NNH_TEST(names_are_kept_in_debug_and_development_only) {
    PrefabLibrary lib = make_library();
    const Prefab p = must_add(lib, "flying_walker", {}, Coin{1});
    const Prefab q = must_add(lib, "a_name_longer_than_thirty_one_bytes_is_cut", {}, Coin{1});
    if (nnh::build::kDevToolsEnabled) {
        CHECK(std::string_view(lib.name(p)) == "flying_walker");
        CHECK(std::string_view(lib.name(q)) == "a_name_longer_than_thirty_one_b");
        CHECK(lib.find("a_name_longer_than_thirty_one_bytes_is_cut") == q); // found by the whole name's hash
    } else {
        CHECK(std::string_view(lib.name(p)).empty() && std::string_view(lib.name(q)).empty());
    }
    CHECK(std::string_view(lib.name(Prefab{})).empty());
}
