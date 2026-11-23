#pragma once
// The prefabs example's components (spec 021 AC-5): the game's own plain structs. Body carries the placement member
// (`box`: a touch or a map object puts the copy there).

#include <nnh/gfx.hpp>
#include <nnh/math.hpp>
#include <nnh/tween.hpp>

namespace room {

struct Body {
    nnh::Rect box{};
    nnh::Color color{255, 255, 255, 255};
};
/// Pixels per frame along x; turns at the room's walls.
struct Walker {
    float vx = 0.5f;
};
/// A flying walker's height above its body, moved by a yoyo tween.
struct Bob {
    float offset = 0;
    nnh::TweenHandle tween{};
};
struct Coin {
    int value = 1;
};
/// Hit points (the roster's enemies, from their data assets).
struct Stats {
    int hp = 1;
};

inline constexpr int kMaxEntities = 64;
inline constexpr int kMaxWalkers = 16; ///< the walker kind's budget: touches past it are misses
inline constexpr int kMaxCoins = 24;
inline constexpr nnh::Rect kInside{16, 16, 368, 208}; ///< the room inside its walls

} // namespace room
