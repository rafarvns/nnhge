#pragma once
// Shared helpers for the prefab tests (spec 021): padding-free component kinds, a counting kind, libraries and worlds
// that must be created, the block hash (a refused registration changes nothing) and the crash helpers for misuse.

#include <cstdint>
#include <type_traits>
#include <utility>

#include "../entities/world_fixture.hpp"
#include "nnh/math.hpp"
#include "nnh/prefabs.hpp"
#include "prefabs/prefab_internal.hpp"
#include "test.hpp"

namespace prefab_test {

using world_test::Counted;

struct Body {
    nnh::Rect box{};
    std::uint32_t layers = 0;
};
struct Coin {
    std::int32_t value = 1;
};
struct Big {
    std::int32_t v[16] = {};
};
struct Mover {
    nnh::Vec2 at{};
    nnh::Vec2 v{};
};
struct Bob {
    std::int32_t phase = 0;
};
static_assert(std::has_unique_object_representations_v<Coin> && std::has_unique_object_representations_v<Big> &&
              std::has_unique_object_representations_v<Bob>);

/// Distinct kinds for capacity tests: K<0>, K<1>, ...
template <int N>
struct K {
    std::int32_t v = N;
};

inline constexpr nnh::Tag kEnemy = nnh::tag("enemy");
inline constexpr nnh::Tag kCoinTag = nnh::tag("coin");
inline constexpr nnh::Tag kFlying = nnh::tag("flying");
inline constexpr nnh::Tag kFx = nnh::tag("fx");
static_assert(nnh::tags_distinct(kEnemy, kCoinTag, kFlying, kFx));

/// A library that must be created.
inline nnh::PrefabLibrary make_library(const nnh::PrefabLibraryDesc& desc = {.max_prefabs = 16, .max_entries = 64,
                                                                              .value_bytes = 1024}) {
    auto r = nnh::PrefabLibrary::create(desc);
    CHECK(r.ok());
    return r ? std::move(r.value()) : nnh::PrefabLibrary{};
}

/// A registration that must succeed.
template <typename... T>
nnh::Prefab must_add(nnh::PrefabLibrary& lib, const char* name, const nnh::PrefabOptions& options, const T&... values) {
    auto r = lib.add(name, options, values...);
    CHECK(r.ok());
    return r ? r.value() : nnh::Prefab{};
}

/// A world of Body, Coin, Big, Mover, Bob and Counted kinds (`cap` each) with `tags` tags per entity.
inline nnh::World make_world(int entities = 32, int cap = 32, int tags = 4) {
    const nnh::ComponentDesc kinds[] = {nnh::component<Body>(cap),  nnh::component<Coin>(cap),
                                        nnh::component<Big>(cap),   nnh::component<Mover>(cap),
                                        nnh::component<Bob>(cap),   nnh::component<Counted>(cap)};
    auto r = nnh::World::create({.max_entities = entities, .tags_per_entity = tags}, kinds);
    CHECK(r.ok());
    return static_cast<nnh::World&&>(r.value());
}

/// FNV-1a of a library's whole block (header included).
inline std::uint32_t block_hash(const nnh::PrefabLibrary& lib) {
    const nnh::detail::PrefabBlock* b = nnh::PrefabAccess::block(lib);
    if (b == nullptr) return 0;
    const auto* bytes = reinterpret_cast<const unsigned char*>(b);
    const auto size = static_cast<std::size_t>(b->values + b->value_capacity - bytes);
    std::uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) h = (h ^ bytes[i]) * 16777619u;
    return h;
}

inline nnh::ErrorCode add_error(nnh::Result<nnh::Prefab> r) { return r ? nnh::ErrorCode::None : r.error().code; }

using world_test::asserts_or_is_ignored;
using world_test::crashes;

} // namespace prefab_test
