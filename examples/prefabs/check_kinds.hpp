#pragma once
// The self-check's component kinds, tags and prefab table (spec 021 AC-2): five padding-free kinds (A carries a Rect
// placement member, C a Vec2 one, D has a destructor), six tags, and the 16 prefabs written out flattened by hand —
// what the hand-written side spawns, never read from the library.

#include <cstdint>
#include <nnh/entities.hpp>
#include <nnh/math.hpp>

namespace check {

struct A {
    nnh::Rect box{};
    std::uint32_t layers = 0;
};
struct B {
    std::int32_t v[16] = {};
};
struct C {
    nnh::Vec2 v{};
};
struct D {
    std::uint32_t v = 0;
    D() = default;
    explicit D(std::uint32_t value) : v(value) {}
    D(const D&) = default;
    D& operator=(const D&) = default;
    ~D() {} // user-provided: the destroy path of a copy
};
struct E {
    std::uint16_t a = 0, b = 0;
};
// No padding: "every component byte" of the state hash is a value byte.
static_assert(sizeof(A) == 20 && sizeof(B) == 64 && sizeof(C) == 8 && sizeof(D) == 4 && sizeof(E) == 4);

enum Kind : int { kA, kB, kC, kD, kE, kKinds };
inline constexpr int kCapacity[kKinds] = {1024, 300, 1024, 1024, 200};

inline constexpr nnh::Tag kTags[6] = {nnh::tag("enemy"), nnh::tag("coin"), nnh::tag("flying"),
                                      nnh::tag("fx"),    nnh::tag("boss"), nnh::tag("pickup")};
static_assert(nnh::tags_distinct(kTags[0], kTags[1], kTags[2], kTags[3], kTags[4], kTags[5]));
enum TagIndex : int { kEnemy, kCoin, kFlying, kFx, kBoss, kPickup };

/// B{n}: v[i] = n·100 + i.
B b_seq(int n);

/// One value per kind (a prefab's values, or a copy's overrides).
struct Values {
    A a;
    B b;
    C c;
    D d;
    E e;
};

enum class Place : std::uint8_t { None, ABox, CVec };
enum Hook : int { kHookInfo, kHookMark, kHookChild, kHookSuicide };

/// A prefab flattened: entries in order, their values, tags, placement and hooks in chain order (base first).
struct HandDef {
    int kinds[kKinds] = {};
    int kind_count = 0;
    Values v{};
    int tags[4] = {};
    int tag_count = 0;
    Place place = Place::None;
    int hooks[3] = {};
    int hook_count = 0;
};

inline constexpr int kPrefabs = 16;
inline constexpr int kChild = 15;  ///< the prefab kHookChild spawns
inline constexpr int kDSpawn = 13; ///< the prefab D's remove hook spawns

/// The 16 definitions, flattened by hand (self_check_defs.cpp).
const HandDef& hand_def(int index);

} // namespace check
