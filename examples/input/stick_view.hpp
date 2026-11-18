#pragma once
// Draws an analog stick as a box with a dot, plus its values (debug text only: no sprites yet).

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

/// Box of side 2 * kStickRadius centered on (center_x, center_y), the dot at the stick's position.
inline constexpr int kStickRadius = 36;

void draw_stick(nnh::Screen screen, int center_x, int center_y, const char* label, nnh::Stick stick,
                bool available = true);
