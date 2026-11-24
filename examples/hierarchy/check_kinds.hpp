#pragma once
// The hierarchy self-check's kinds and tags (spec 022 AC-2): the engine's Node, A (a box), B (four words), C (with a
// user-provided destructor), D (its remove hook destroys or attaches other entities), K (a serial on every entity, its
// hook counting quarantined entities). Padding-free, so a component's bytes are its value.

#include <cstdint>

#include "nnh/entities.hpp"
#include "nnh/hierarchy.hpp"

namespace hcheck {

struct A {
    nnh::Rect box{};
    std::uint32_t layers = 0;
};
struct B {
    std::uint32_t v[4] = {};
};
struct C {
    std::uint16_t a = 0, b = 0;
    ~C() { a = b = 0; } // user-provided: copies are constructed, never assigned
};
struct D {
    std::uint32_t v = 0;
};
struct K {
    std::uint32_t serial = 0;
};

enum Kind : int { kNode = 0, kA, kB, kC, kD, kK, kKinds };
inline constexpr int kCapacity[kKinds] = {768, 512, 300, 1024, 1024, 1024};
inline constexpr int kEntities = 1024;
inline constexpr int kLiveLimit = 900;

inline constexpr nnh::Tag kTags[4] = {nnh::tag("t0"), nnh::tag("t1"), nnh::tag("t2"), nnh::tag("t3")};
static_assert(nnh::tags_distinct(kTags[0], kTags[1], kTags[2], kTags[3]));

inline constexpr int kPrefabs = 12;
/// The prefabs, in registration order.
enum Def : int { kBare, kNodeA, kMuzzle, kGun, kShield, kShip, kShielded, kTower, kRing, kSwarm, kPlain, kSpawner };
/// Hook ids (0: none).
enum Hook : int { kNoHook, kShipHook, kSpawnerHook, kTowerHook };

} // namespace hcheck
