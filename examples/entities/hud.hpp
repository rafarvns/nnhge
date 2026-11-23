#pragma once
// The entities example's HUD (bottom screen, in screen pixels over the arena's view): the world's stats and each
// kind's (spec 020 FR-24), the bullet pool, the tag filter and the missile's state.

#include "arena.hpp"

namespace arena {

/// `filter`: the tag cycled by X (Tag::None: no filter); `found`: find()'s total for it.
void draw_hud(Arena& arena, nnh::Tag filter, int found);

} // namespace arena
