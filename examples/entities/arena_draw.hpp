#pragma once
// Drawing the arena with shapes only (no art): walls, bodies coloured by kind, bob trails, explosions, bullets. The
// same drawing serves both screens (each with its own camera). Drawing reads the world through iterations and
// storage views; it changes nothing.

#include "arena.hpp"

#include <nnh/gfx.hpp>

namespace arena {

/// Draws the arena on `screen` (camera set by the caller). `highlight`: entities to outline (a tag filter's find).
void draw_arena(nnh::Screen screen, Arena& arena, const nnh::Entity* highlight, int highlighted);

} // namespace arena
