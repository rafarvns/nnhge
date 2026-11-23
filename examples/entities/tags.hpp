#pragma once
// The entities example's tags (spec 020 FR-16): hashed at compile time, checked for collisions once.

#include <nnh/entities.hpp>

namespace arena {

inline constexpr nnh::Tag kEnemy = nnh::tag("enemy");
inline constexpr nnh::Tag kFlying = nnh::tag("flying");
inline constexpr nnh::Tag kCoin = nnh::tag("coin");
inline constexpr nnh::Tag kFx = nnh::tag("fx");
inline constexpr nnh::Tag kFrozen = nnh::tag("frozen");
static_assert(nnh::tags_distinct(kEnemy, kFlying, kCoin, kFx, kFrozen), "two tags of the example share a hash");

/// The name of a tag of this example ("?" for others): tags store no names, the game keeps its own.
inline const char* tag_name(nnh::Tag t) {
    switch (t) {
    case kEnemy:
        return "enemy";
    case kFlying:
        return "flying";
    case kCoin:
        return "coin";
    case kFx:
        return "fx";
    case kFrozen:
        return "frozen";
    default:
        return "?";
    }
}

} // namespace arena
