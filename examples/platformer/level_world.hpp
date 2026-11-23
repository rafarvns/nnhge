#pragma once
// The platformer's entities (spec 020 AC-4): the components of a level's world, its tags and its description. The
// player, the coins and the turrets are entities; each has a Body (a box on collision layers, spec 019) that the
// coins' overlap query and the bullets' rays read in place through a box view of the body storage. Free and
// destroyed body slots hold Body{} — no layers — so no query ever sees them.

#include "layers.hpp"

#include <nnh/collision.hpp>
#include <nnh/entities.hpp>
#include <nnh/math.hpp>
#include <nnh/result.hpp>

namespace level {

/// Where a thing is and what it is, for the collision queries.
struct Body {
    nnh::Rect box{};
    nnh::collision::Layers layers = nnh::collision::kNoLayers;
};
/// What a coin is worth.
struct Coin {
    int value = 1;
};
/// A turret's sight and its frames until it may fire again (turrets.hpp).
struct Turret {
    bool sees = false;
    int cooldown = 0;
};

inline constexpr nnh::Tag kPlayerTag = nnh::tag("player");
inline constexpr nnh::Tag kCoinTag = nnh::tag("coin");
inline constexpr nnh::Tag kTurretTag = nnh::tag("turret");
static_assert(nnh::tags_distinct(kPlayerTag, kCoinTag, kTurretTag));

inline constexpr int kMaxEntities = 256;
inline constexpr int kMaxCoins = 128;
inline constexpr int kMaxTurrets = 4;

/// A level's world: Body, Coin and Turret kinds, two tags per entity. Fails only when out of memory.
nnh::Result<nnh::World> create_world();

} // namespace level
