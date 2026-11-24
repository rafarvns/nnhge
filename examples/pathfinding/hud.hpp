#pragma once
// The showcase's text (spec 025 AC-4): the last search's status, cost, cells, expansions and time on the top screen;
// the field's progress, the settings and the controls on the bottom screen.

#include "crowd.hpp"
#include "debug_view.hpp"
#include "hero.hpp"
#include "level.hpp"

void draw_hud(const Level& level, const Hero& hero, const Crowd& crowd, debug_view::Mode mode);
