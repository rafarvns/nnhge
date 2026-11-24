#pragma once
// The platformer's entities (spec 020 AC-4): the components of a level's world, its tags and its description. The
// player, the coins and the turrets are entities; each has a Body (a box on collision layers, spec 019) that the
// coins' overlap query and the bullets' rays read in place through a box view of the body storage. Free and
// destroyed body slots hold Body{} — no layers — so no query ever sees them.

#include "layers.hpp"

#include <nnh/collision.hpp>
#include <nnh/entities.hpp>
#include <nnh/hierarchy.hpp>
#include <nnh/math.hpp>
#include <nnh/result.hpp>
#include <nnh/tween.hpp>

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
/// A coin's sparkle (sparkles.hpp, spec 020 AC-4 step 2): faded by a tween, destroyed when the fade ends.
struct Spark {
    nnh::Vec2 position{};
    float alpha = 0;
    nnh::TweenHandle fade{};
};
/// A turret's barrel (spec 022 AC-4): a child node of its turret, turned toward the player while the turret sees it.
struct Barrel {
    int frame = 0;  ///< the barrel's image (one for now)
    bool lit = false; ///< drawn lit while its turret sees the player
};
/// Pixels per frame.
struct Velocity {
    nnh::Vec2 v{};
};

inline constexpr nnh::Tag kPlayerTag = nnh::tag("player");
inline constexpr nnh::Tag kCoinTag = nnh::tag("coin");
inline constexpr nnh::Tag kTurretTag = nnh::tag("turret");
inline constexpr nnh::Tag kFxTag = nnh::tag("fx");
inline constexpr nnh::Tag kBarrelTag = nnh::tag("barrel");
static_assert(nnh::tags_distinct(kPlayerTag, kCoinTag, kTurretTag, kFxTag, kBarrelTag));

inline constexpr int kMaxEntities = 256;
inline constexpr int kMaxCoins = 128;
inline constexpr int kMaxTurrets = 4;
inline constexpr int kMaxSparks = 96;
inline constexpr int kMaxNodes = 8; ///< the turrets and their barrels (spec 022)

/// A level's world: Body, Coin, Turret, Spark and Velocity kinds, then (spec 022: registered last, so the others keep
/// their indices) the engine's Node and Barrel; two tags per entity. Spark's remove hook kills its fade in `tweens` (when
/// given: bind the world before that set). Fails only when out of memory.
nnh::Result<nnh::World> create_world(nnh::TweenSet* tweens = nullptr);

} // namespace level
