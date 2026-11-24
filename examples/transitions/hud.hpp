#pragma once
// The bottom screen's HUD: the room's map, the settings and the transition queries (spec 023 FR-15).

#include <nnh/math.hpp>

#include "options_menu.hpp"

void draw_hud(const Settings& settings, int room, nnh::Vec2 square);
