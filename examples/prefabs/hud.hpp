#pragma once
// The bottom screen's text (spec 021 AC-5, FR-11): the library's stats, the last map summary, the selected prefab with
// its kinds and tags (the debug line), the roster from the data assets, and the controls.

#include "room.hpp"

namespace room {

void draw_hud(Room& room);

} // namespace room
