#pragma once
// Draws the room (spec 021 AC-5): the Tiled map's tiles, then each body (flying walkers lifted by their Bob, with a
// shadow). The bottom screen shows the same world through its camera.

#include "room.hpp"

namespace room {

void draw_room(nnh::Screen screen, Room& room);

} // namespace room
