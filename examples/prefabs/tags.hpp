#pragma once
// The prefabs example's tags (spec 020 tags, named once; their hashes checked distinct at compile time).

#include <nnh/entities.hpp>

namespace room {

inline constexpr nnh::Tag kEnemy = nnh::tag("enemy");
inline constexpr nnh::Tag kFlying = nnh::tag("flying");
inline constexpr nnh::Tag kCoin = nnh::tag("coin");
inline constexpr nnh::Tag kRoster = nnh::tag("roster");
static_assert(nnh::tags_distinct(kEnemy, kFlying, kCoin, kRoster));

/// The name of one of the tags above ("?" for another).
inline const char* tag_name(nnh::Tag t) {
    if (t == kEnemy) return "enemy";
    if (t == kFlying) return "flying";
    if (t == kCoin) return "coin";
    if (t == kRoster) return "roster";
    return "?";
}

} // namespace room
