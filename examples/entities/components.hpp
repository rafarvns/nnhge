#pragma once
// The entities example's components (spec 020 AC-5): the game's own plain structs. The engine ships none of them.
// Free and destroyed slots hold a value-initialised T{}: Body's layers default to kNoLayers, so a box view of the body
// storage never sees them.

#include <nnh/collision.hpp>
#include <nnh/entities.hpp>
#include <nnh/math.hpp>
#include <nnh/tween.hpp>

namespace arena {

/// Collision layers of the arena (spec 019): what a body is.
namespace bits {
inline constexpr nnh::collision::Layers kEnemy = nnh::collision::layer(1);
inline constexpr nnh::collision::Layers kCoin = nnh::collision::layer(2);
inline constexpr nnh::collision::Layers kShip = nnh::collision::layer(3);
inline constexpr nnh::collision::Layers kMissile = nnh::collision::layer(4);
} // namespace bits

/// Where a thing is, for drawing and for the collision queries.
struct Body {
    nnh::Rect box{};
    nnh::collision::Layers layers = nnh::collision::kNoLayers;
};
/// Pixels per frame.
struct Velocity {
    nnh::Vec2 v{};
};
/// A flyer's vertical bob: `offset` is written by the scene's tween set through a handle-capturing setter.
struct Bob {
    float base_y = 0;
    float offset = 0;
    nnh::TweenHandle tween{};
};
struct Coin {
    int value = 1;
};
/// A homing missile: the handle of what it chases (it may go stale).
struct Missile {
    nnh::Entity target{};
};
/// A short-lived burst where something was destroyed.
struct Explosion {
    nnh::Vec2 at{};
    int frames = 0;
};

/// A bullet of the ship's pool (not an entity: a pool is enough for one kind of thing).
struct Bullet {
    nnh::Vec2 position{};
};

} // namespace arena
